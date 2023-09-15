#pragma once

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"
#include "envoy/server/factory_context.h"

#include "source/common/common/logger.h"
#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

/**
 * @brief A Dynatrace specific sampler *
 */
class DynatraceSampler : public Sampler, Logger::Loggable<Logger::Id::tracing> {
public:
  explicit DynatraceSampler(
      const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config)
      : tenant_id_(config.tenant_id()), cluster_id_(config.cluster_id()), counter_(0) {}

  SamplingResult shouldSample(const absl::StatusOr<SpanContext>& parent_context,
                              const std::string& trace_id, const std::string& name,
                              ::opentelemetry::proto::trace::v1::Span::SpanKind spankind,
                              const std::map<std::string, std::string>& attributes,
                              const std::set<SpanContext> links) override;

  std::string getDescription() const override;

private:
  std::string tenant_id_;
  std::string cluster_id_;
  std::atomic<uint32_t> counter_;

  DynatraceTracestate createTracestate(bool is_recording, std::string sampling_exponent);
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
