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

FW4Tag DynatraceSampler::getFW4Tag(const Tracestate& tracestate) {
  for (auto const& entry : tracestate.entries()) {
    if (dynatrace_tracestate_.keyMatches(
            entry.key)) { // found a tracestate entry with key matching our tenant/cluster
      return FW4Tag::create(entry.value);
    }
  }
  return FW4Tag::createInvalid();
}

/*
OTelSpanAttributes getInitialAttributes(const Tracing::TraceContext& trace_context,
                                        OTelSpanKind span_kind) {
  OTelSpanAttributes attributes;

  // required attributes for a server span are defined in
  // https://github.com/open-telemetry/semantic-conventions/blob/main/docs/http/http-spans.md
  if (span_kind == ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER) {
    std::vector<absl::string_view> address_port =
        absl::StrSplit(trace_context.host(), ":", absl::SkipEmpty());
    if (address_port.size() > 0) {
      attributes["server.address"] = address_port[0];
    }
    if (address_port.size() > 1) {
      attributes["server.port"] = address_port[1];
    }
    std::vector<absl::string_view> path_query =
        absl::StrSplit(trace_context.path(), "?", absl::SkipEmpty());
    if (path_query.size() > 0) {
      attributes["url.path"] = path_query[0];
    }
    if (path_query.size() > 1) {
      attributes["url.query"] = path_query[1];
    }
    auto scheme = trace_context.getByKey(":scheme");
    if (scheme.has_value()) {
      attributes["url.scheme"] = scheme.value();
    }
    if (!trace_context.method().empty()) {
      attributes["http.request.method"] = trace_context.method();
    }
  }
  return attributes;
}
*/

DynatraceSampler::DynatraceSampler(
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config,
    Server::Configuration::TracerFactoryContext& context)
    : tenant_id_(config.tenant_id()), cluster_id_(config.cluster_id()),
      dynatrace_tracestate_(tenant_id_, cluster_id_), tracer_factory_context_(context),
      counter_(0) {
  timer_ = tracer_factory_context_.serverFactoryContext().mainThreadDispatcher().createTimer(
      [this]() -> void {
        ENVOY_LOG(info, "HELLO FROM TIMER");
        timer_->enableTimer(std::chrono::seconds(60));
      });
  timer_->enableTimer(std::chrono::seconds(10));
}

SamplingResult DynatraceSampler::shouldSample(const absl::optional<SpanContext> parent_context,
                                              const std::string& /*trace_id*/,
                                              const std::string& /*name*/, OTelSpanKind /*kind*/,
                                              OptRef<const Tracing::TraceContext> trace_context,
                                              const std::vector<SpanContext>& /*links*/) {

  SamplingResult result;
  std::map<std::string, std::string> att;

  if (trace_context.has_value()) {
    ENVOY_LOG(info, "trace_context->path:\t{}", trace_context->path());
    ENVOY_LOG(info, "trace_context->method():\t{}", trace_context->method());
    ENVOY_LOG(info, "trace_context->host():\t{}", trace_context->host());
  }
  auto current = tracer_factory_context_.serverFactoryContext()
                     .timeSource()
                     .monotonicTime()
                     .time_since_epoch();

  auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current);
  ENVOY_LOG(info, "monotonicTime:\t{}", current_ms.count());

  Tracestate tracestate;
  tracestate.parse(parent_context.has_value() ? parent_context->tracestate() : "");

  FW4Tag fw4_tag = getFW4Tag(tracestate);

  if (fw4_tag.isValid()) { // we found a trace decision in tracestate header
    result.decision = fw4_tag.isIgnored() ? Decision::DROP : Decision::RECORD_AND_SAMPLE;
    att[SAMPLING_EXTRAPOLATION_SPAN_ATTRIBUTE_NAME] = std::to_string(fw4_tag.getSamplingExponent());
    result.tracestate = parent_context->tracestate();

  } else {
    // make a sampling decision
    uint32_t current_counter = counter_++;
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

    FW4Tag new_tag = FW4Tag::create(!sample, sampling_exponent);
    tracestate.add(dynatrace_tracestate_.getKey(), new_tag.createValue());
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
