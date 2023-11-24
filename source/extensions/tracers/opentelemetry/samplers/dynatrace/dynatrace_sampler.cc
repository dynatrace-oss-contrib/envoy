#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"

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

FW4Tag DynatraceSampler::getFW4Tag(const Tracestate& tracestate) {
  for (auto const& entry : tracestate.entries()) {
    if (dt_tracestate_entry_.keyMatches(
            entry.key)) { // found a tracestate entry with key matching our tenant/cluster
      return FW4Tag::create(entry.value);
    }
  }
  return FW4Tag::createInvalid();
}

DynatraceSampler::DynatraceSampler(
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
    Server::Configuration::TracerFactoryContext& /*context*/)
    : tenant_id_(config.tenant_id()), cluster_id_(config.cluster_id()),
      dt_tracestate_entry_(tenant_id_, cluster_id_), counter_(0) {}

SamplingResult DynatraceSampler::shouldSample(const absl::optional<SpanContext> parent_context,
                                              const std::string& /*trace_id*/,
                                              const std::string& /*name*/, OTelSpanKind /*kind*/,
                                              OptRef<const Tracing::TraceContext> /*trace_context*/,
                                              const std::vector<SpanContext>& /*links*/) {

  SamplingResult result;
  std::map<std::string, std::string> att;
  // search for an existing forward tag in the tracestate
  Tracestate tracestate;
  tracestate.parse(parent_context.has_value() ? parent_context->tracestate() : "");

  if (FW4Tag fw4_tag = getFW4Tag(tracestate);
      fw4_tag.isValid()) { // we found a trace decision in tracestate header
    result.decision = fw4_tag.isIgnored() ? Decision::DROP : Decision::RECORD_AND_SAMPLE;
    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = std::to_string(fw4_tag.getSamplingExponent());
    result.tracestate = parent_context->tracestate();

  } else { // make a sampling decision
    // this is just a demo, we sample every second request here
    uint32_t current_counter = ++counter_;
    bool sample;
    int sampling_exponent;
    if (current_counter % 2) {
      sample = true;
      sampling_exponent = 1;
    } else {
      sample = false;
      sampling_exponent = 0;
    }

    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = std::to_string(sampling_exponent);

    result.decision = sample ? Decision::RECORD_AND_SAMPLE : Decision::DROP;
    // create new forward tag and add it to tracestate
    FW4Tag new_tag = FW4Tag::create(!sample, sampling_exponent);
    tracestate.add(dt_tracestate_entry_.getKey(), new_tag.asString());
    result.tracestate = tracestate.asString();
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
