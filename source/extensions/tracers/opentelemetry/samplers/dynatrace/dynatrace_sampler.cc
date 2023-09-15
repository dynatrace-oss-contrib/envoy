#include "dynatrace_sampler.h"

#include <memory>
#include <sstream>
#include <string>

#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

#include "murmur.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

static const char* SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME =
    "sampling_extrapolation_set_in_sampler";

DynatraceTracestate DynatraceSampler::createTracestate(bool is_recording,
                                                       std::string sampling_exponent) {
  DynatraceTracestate tracestate;
  tracestate.sampling_exponent = sampling_exponent;
  tracestate.tenant_id = tenant_id_;
  tracestate.cluster_id = cluster_id_;
  tracestate.is_ignored = is_recording ? "0" : "1";
  return tracestate;
}

SamplingResult
DynatraceSampler::shouldSample(const absl::StatusOr<SpanContext>& parent_context,
                               const std::string& /*trace_id*/, const std::string& /*name*/,
                               ::opentelemetry::proto::trace::v1::Span::SpanKind /*spankind*/,
                               const std::map<std::string, std::string>& /*attributes*/,
                               const std::set<SpanContext> /*links*/) {

  SamplingResult result;
  std::map<std::string, std::string> att;
  // uint32_t current_counter = counter_++;
  DynatraceTracestate tracestate;

  if (parent_context.ok()) { // there is already a trace,
    // TODO: we should check if there is a dynatrace sampling decision on the state and use it
    result.decision = Decision::RECORD_AND_SAMPLE;
    // Expects a tracestate like
    // "<tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;8;<rootPathRandom>;<extensionChecksum>"
    tracestate = DynatraceTracestate::parse(parent_context->tracestate());
    if (parent_context->tracestate()
            .empty()) { // TODO, we should if there isn't a dynatrace tracestate
      result.tracestate = createTracestate(result.isRecording(), "0").toString();
    } else {
      result.tracestate = parent_context->tracestate();
    }
  } else { // start new trace
           // if (current_counter % 2 == 0) {
    result.decision = Decision::RECORD_AND_SAMPLE;
    // } else {
    //   result.decision = Decision::RECORD_ONLY;
    //   if (parent_context.ok()) {
    //     result.tracestate = parent_context.value().tracestate();
    //   }
    // }

    tracestate = createTracestate(result.isRecording(), "0");
    result.tracestate = tracestate.toString();
  }

  att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = tracestate.sampling_exponent;
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
