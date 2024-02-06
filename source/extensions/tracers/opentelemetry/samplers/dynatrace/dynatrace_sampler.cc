#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"

#include <memory>
#include <sstream>
#include <string>

#include "source/common/common/hash.h"
#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"
#include "source/extensions/tracers/opentelemetry/trace_state.h"

#include "absl/strings/str_cat.h"
#include "openssl/md5.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

constexpr std::chrono::minutes SAMPLING_UPDATE_TIMER_DURATION{1};
const char* SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME = "sampling_extrapolation_set_in_sampler";

absl::Hex calculateTenantId(std::string tenant_uuid) {
  if (tenant_uuid.empty()) {
    return absl::Hex(0);
  }

  for (char& c : tenant_uuid) {
    if (c & 0x80) {
      c = 0x3f; // '?'
    }
  }

  uint8_t digest[16];
  MD5(reinterpret_cast<const uint8_t*>(tenant_uuid.data()), tenant_uuid.size(), digest);

  int32_t hash = 0;
  for (int i = 0; i < 16; i++) {
    const int shift_for_target_byte = (3 - (i % 4)) * 8;
    // 24, 16, 8, 0 respectively
    hash ^=
        (static_cast<int>(digest[i]) << shift_for_target_byte) & (0xff << shift_for_target_byte);
  }
  return absl::Hex(hash);
}

} // namespace

DynatraceSampler::DynatraceSampler(
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
    Server::Configuration::TracerFactoryContext& context,
    SamplerConfigFetcherPtr sampler_config_fetcher)
    : dt_tracestate_key_(absl::StrCat(calculateTenantId(config.tenant()), "-",
                                      absl::string_view(config.cluster_id()), "@dt")),
      sampling_controller_(std::move(sampler_config_fetcher)) {

  timer_ = context.serverFactoryContext().mainThreadDispatcher().createTimer([this]() -> void {
    sampling_controller_.update();
    timer_->enableTimer(SAMPLING_UPDATE_TIMER_DURATION);
  });
  timer_->enableTimer(SAMPLING_UPDATE_TIMER_DURATION);
}

SamplingResult DynatraceSampler::shouldSample(const absl::optional<SpanContext> parent_context,
                                              const std::string& trace_id,
                                              const std::string& /*name*/, OTelSpanKind /*kind*/,
                                              OptRef<const Tracing::TraceContext> trace_context,
                                              const std::vector<SpanContext>& /*links*/) {

  SamplingResult result;
  std::map<std::string, std::string> att;

  // trace_context->path() returns path and query. query part is removed in getSamplingKey()
  const std::string sampling_key =
      trace_context.has_value()
          ? sampling_controller_.getSamplingKey(trace_context->path(), trace_context->method())
          : "";

  sampling_controller_.offer(sampling_key);

  auto trace_state =
      TraceState::fromHeader(parent_context.has_value() ? parent_context->tracestate() : "");

  std::string trace_state_value;

  if (trace_state->get(dt_tracestate_key_, trace_state_value)) {
    // we found a DT trace decision in tracestate header
    if (FW4Tag fw4_tag = FW4Tag::create(trace_state_value); fw4_tag.isValid()) {
      result.decision = fw4_tag.isIgnored() ? Decision::Drop : Decision::RecordAndSample;
      att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] =
          std::to_string(fw4_tag.getSamplingExponent());
      result.tracestate = parent_context->tracestate();
    }
  } else {
    // do a decision based on the calculated exponent
    // at the moment we use a hash of the trace_id as random number
    const auto hash = MurmurHash::murmurHash2(trace_id);
    const auto sampling_state = sampling_controller_.getSamplingState(sampling_key);
    const bool sample = sampling_state.shouldSample(hash);
    const auto sampling_exponent = sampling_state.getExponent();

    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = std::to_string(sampling_exponent);

    result.decision = sample ? Decision::RecordAndSample : Decision::Drop;
    // create new forward tag and add it to tracestate
    FW4Tag new_tag = FW4Tag::create(!sample, sampling_exponent, static_cast<uint8_t>(hash));
    trace_state = trace_state->set(dt_tracestate_key_, new_tag.asString());
    result.tracestate = trace_state->toHeader();
  }

  if (!att.empty()) {
    result.attributes = std::make_unique<const std::map<std::string, std::string>>(std::move(att));
  }
  return result;
}

std::string DynatraceSampler::getDescription() const { return "DynatraceSampler"; }

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
