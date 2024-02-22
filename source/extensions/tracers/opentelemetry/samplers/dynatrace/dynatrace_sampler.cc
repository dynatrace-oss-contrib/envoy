#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"

#include <memory>
#include <sstream>
#include <string>

#include "source/common/common/hash.h"
#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/tenant_id.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

#include "absl/strings/str_cat.h"
#include "opentelemetry/trace/trace_state.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

constexpr std::chrono::minutes SAMPLING_UPDATE_TIMER_DURATION{1};

class DynatraceTag {
public:
  static DynatraceTag createInvalid() { return {false, false, 0, 0}; }

  static DynatraceTag create(bool ignored, uint32_t sampling_exponent, uint32_t path_info) {
    return {true, ignored, sampling_exponent, path_info};
  }

  static DynatraceTag create(const std::string& value) {
    std::vector<absl::string_view> tracestate_components =
        absl::StrSplit(value, ';', absl::AllowEmpty());
    if (tracestate_components.size() < 8) {
      return createInvalid();
    }

    if (tracestate_components[0] != "fw4") {
      return createInvalid();
    }
    bool ignored = tracestate_components[5] == "1";
    uint32_t sampling_exponent;
    uint32_t path_info;
    if (absl::SimpleAtoi(tracestate_components[6], &sampling_exponent) &&
        absl::SimpleHexAtoi(tracestate_components[7], &path_info)) {
      return {true, ignored, sampling_exponent, path_info};
    }
    return createInvalid();
  }

  std::string asString() const {
    std::string ret = absl::StrCat("fw4;0;0;0;0;", ignored_ ? "1" : "0", ";", sampling_exponent_,
                                   ";", absl::Hex(path_info_));
    return ret;
  }

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  int getSamplingExponent() const { return sampling_exponent_; };
  uint32_t getPathInfo() const { return path_info_; };

private:
  DynatraceTag(bool valid, bool ignored, uint32_t sampling_exponent, uint32_t path_info)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent),
        path_info_(path_info) {}

  bool valid_;
  bool ignored_;
  uint32_t sampling_exponent_;
  uint32_t path_info_;
};

// add Dynatrace specific span attributes
void addSamplingAttributes(uint32_t sampling_exponent,
                           std::map<std::string, std::string>& attributes) {

  const auto multiplicity = SamplingState::toMultiplicity(sampling_exponent);
  // The denominator of the sampling ratio. If, for example, the Dynatrace OneAgent samples with a
  // probability of 1/16, the value of supportability.atm_sampling_ratio would be 16.
  // Note: Ratio is also known as multiplicity.
  attributes["supportability.atm_sampling_ratio"] = std::to_string(multiplicity);

  if (multiplicity > 1) {
    static constexpr uint64_t two_pow_56 = 1lu << 56; // 2^56
    // The sampling probability can be interpreted as the number of spans
    // that are discarded out of 2^56. The attribute is only available if the sampling.threshold is
    // not 0 and therefore sampling happened.
    const uint64_t sampling_threshold = two_pow_56 - two_pow_56 / multiplicity;
    attributes["sampling.threshold"] = std::to_string(sampling_threshold);
  }
}

} // namespace

DynatraceSampler::DynatraceSampler(
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
    Server::Configuration::TracerFactoryContext& context,
    SamplerConfigFetcherPtr sampler_config_fetcher)
    : dt_tracestate_key_(absl::StrCat(calculateTenantId(config.tenant()), "-",
                                      absl::Hex(config.cluster_id()), "@dt")),
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

  // add it to stream summary containing the number of requests
  sampling_controller_.offer(sampling_key);

  auto trace_state = opentelemetry::trace::TraceState::FromHeader(
      parent_context.has_value() ? parent_context->tracestate() : "");

  std::string trace_state_value;

  if (trace_state->Get(dt_tracestate_key_, trace_state_value)) {
    // we found a DT trace decision in tracestate header
    if (DynatraceTag dynatrace_tag = DynatraceTag::create(trace_state_value);
        dynatrace_tag.isValid()) {
      result.decision = dynatrace_tag.isIgnored() ? Decision::Drop : Decision::RecordAndSample;
      addSamplingAttributes(dynatrace_tag.getSamplingExponent(), att);
      result.tracestate = parent_context->tracestate();
    }
  } else {
    // do a decision based on the calculated exponent
    // at the moment we use a hash of the trace_id as random number
    const auto hash = MurmurHash::murmurHash2(trace_id);
    const auto sampling_state = sampling_controller_.getSamplingState(sampling_key);
    const bool sample = sampling_state.shouldSample(hash);
    const auto sampling_exponent = sampling_state.getExponent();

    addSamplingAttributes(sampling_exponent, att);

    result.decision = sample ? Decision::RecordAndSample : Decision::Drop;
    // create new forward tag and add it to tracestate
    DynatraceTag new_tag =
        DynatraceTag::create(!sample, sampling_exponent, static_cast<uint8_t>(hash));
    trace_state = trace_state->Set(dt_tracestate_key_, new_tag.asString());
    result.tracestate = trace_state->ToHeader();
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
