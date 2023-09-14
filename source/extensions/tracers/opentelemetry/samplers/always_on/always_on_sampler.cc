#include "always_on_sampler.h"

#include <memory>
#include <sstream>
#include <string>

#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

SamplingResult
AlwaysOnSampler::shouldSample(absl::StatusOr<SpanContext>& parent_context,
                              const std::string& trace_id, const std::string& name,
                              ::opentelemetry::proto::trace::v1::Span::SpanKind spankind, 
                              const std::map<std::string, std::string>& attributes) {
  (void)context_;
  (void)trace_id;
  (void)name;
  (void)spankind;
  (void)attributes;
  SamplingResult result;
  result.decision = Decision::RECORD_AND_SAMPLE;
  if (parent_context.ok()) {
    result.trace_state = parent_context.value().tracestate();
  }
  return result;
}

std::string AlwaysOnSampler::getDescription() const {
    return "AlwaysOnSampler";
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy