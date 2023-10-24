#include "dynatrace_sampler.h"

#include <memory>
#include <sstream>
#include <string>

#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/tracestate.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

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

FW4Tag DynatraceSampler::getFW4Tag(const Tracestate& tracestate) {
  for (auto const& entry : tracestate.entries()) {
    if (dynatrace_tracestate_.keyMatches(
            entry.key)) { // found a tracestate entry with key matching our tenant/cluster
      return FW4Tag::create(entry.value);
    }
  }
  return FW4Tag::createInvalid();
}

SamplingResult
DynatraceSampler::shouldSample(const absl::optional<SpanContext> parent_context,
                               const std::string& /*trace_id*/, const std::string& /*name*/,
                               ::opentelemetry::proto::trace::v1::Span::SpanKind /*kind*/,
                               const std::map<std::string, std::string>& /*attributes*/,
                               const std::vector<SpanContext>& /*links*/) {

  SamplingResult result;
  std::map<std::string, std::string> att;

  Tracestate tracestate;
  tracestate.parse(parent_context.has_value() ? parent_context->tracestate() : "");

  FW4Tag fw4_tag = getFW4Tag(tracestate);

  if (fw4_tag.isValid()) { // we found a trace decision in tracestate header
    result.decision = fw4_tag.isIgnored() ? Decision::DROP : Decision::RECORD_AND_SAMPLE;
    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = fw4_tag.getSamplingExponent();
    result.tracestate = parent_context->tracestate();

  } else {                     // make a sampling decision
    bool sample = true;        // TODO
    int sampling_exponent = 8; // TODO
    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = sampling_exponent;

    result.decision = sample ? Decision::RECORD_AND_SAMPLE : Decision::DROP;

    FW4Tag new_tag = FW4Tag::create(!sample, sampling_exponent);
    tracestate.add(dynatrace_tracestate_.getKey(), new_tag.createValue());
    result.tracestate = tracestate.asString();
    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = sampling_exponent;
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
