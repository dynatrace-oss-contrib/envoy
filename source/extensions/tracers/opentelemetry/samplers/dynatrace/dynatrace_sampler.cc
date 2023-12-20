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
#include "stream_summary.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

static constexpr std::chrono::seconds SAMPLING_UPDATE_TIMER_DURATION{20};
// static constexpr std::chrono::minutes SAMPLING_UPDATE_TIMER_DURATION{1};

const char* SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME = "sampling_extrapolation_set_in_sampler";

std::string getSamplingKey(const absl::string_view path_query, const absl::string_view method) {
  size_t query_offset = path_query.find('?');
  auto path =
      path_query.substr(0, query_offset != path_query.npos ? query_offset : path_query.size());
  return absl::StrCat(method, "_", path);
}

} // namespace

DynatraceSampler::DynatraceSampler(
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
    Server::Configuration::TracerFactoryContext& context)
    : tenant_id_(config.tenant_id()), cluster_id_(config.cluster_id()),
      dt_tracestate_entry_(tenant_id_, cluster_id_),
      sampler_config_fetcher_(context, config.http_uri(), config.token()),
      stream_summary_(std::make_unique<StreamSummary<std::string>>(100)), sampling_controller_(),
      counter_(0) {

  timer_ = context.serverFactoryContext().mainThreadDispatcher().createTimer([this]() -> void {
    auto topK = stream_summary_->getTopK();
    // start with a new stream summmary
    stream_summary_ = std::make_unique<StreamSummary<std::string>>(100);

    ENVOY_LOG(info, "Hello from sampler timer. topk.size(): {}", topK.size());
    for (auto const& counter : topK) {
      ENVOY_LOG(info, "-- {} : {}", counter.getItem(), counter.getValue());
    }
    ENVOY_LOG(info, "counter_: {}", counter_);

    timer_->enableTimer(SAMPLING_UPDATE_TIMER_DURATION);
    // update sampling exponents
    sampling_controller_.update(topK,
                                sampler_config_fetcher_.getSamplerConfig().getRootSpansPerMinute());
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

  const std::string sampling_key =
      trace_context.has_value() ? getSamplingKey(trace_context->path(), trace_context->method())
                                : "";

  if (sampling_key != "") {
    Thread::LockGuard lock(mutex_);
    stream_summary_->offer(sampling_key);
  }


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
  } else { // make a sampling decision

    // this is just a demo, we sample every second request here
    // uint32_t current_counter = ++counter_;
    bool sample;
    int sampling_exponent;
    // if (current_counter % 2) {
    //   sample = true;
    //   sampling_exponent = 1;
    // } else {
    //   sample = false;
    //   sampling_exponent = 0;
    // }

    // do a decision based on the calculated exponent
    // at the moment we use a hash of the trace_id as random number
    const auto hash = MurmurHash::murmurHash2(trace_id);
    const auto sampling_state = sampling_controller_.getSamplingState(sampling_key);
    sample = sampling_state.shouldSample(hash);
    sampling_exponent = sampling_state.getExponent();

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
