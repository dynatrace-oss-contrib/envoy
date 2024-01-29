#pragma once

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"
#include "envoy/server/factory_context.h"

#include "source/common/common/logger.h"
#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class FW4Tag {
public:
  static FW4Tag createInvalid() { return {false, false, 0}; }

  static FW4Tag create(bool ignored, int sampling_exponent) {
    return {true, ignored, sampling_exponent};
  }

  static FW4Tag create(const std::string& value) {
    std::vector<absl::string_view> tracestate_components =
        absl::StrSplit(value, ';', absl::AllowEmpty());
    if (tracestate_components.size() < 7) {
      return createInvalid();
    }

    if (tracestate_components[0] != "fw4") {
      return createInvalid();
    }
    bool ignored = tracestate_components[5] == "1";
    int sampling_exponent = std::stoi(std::string(tracestate_components[6]));
    return {true, ignored, sampling_exponent};
  }

  std::string asString() const {
    return absl::StrCat("fw4;0;0;0;0;", ignored_ ? "1" : "0", ";", sampling_exponent_, ";0");
  }

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  int getSamplingExponent() const { return sampling_exponent_; };

private:
  FW4Tag(bool valid, bool ignored, int sampling_exponent)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent) {}

  bool valid_;
  bool ignored_;
  int sampling_exponent_;
};

/**
 * @brief A Dynatrace specific sampler *
 */
class DynatraceSampler : public Sampler, Logger::Loggable<Logger::Id::tracing> {
public:
  DynatraceSampler(
      const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
      Server::Configuration::TracerFactoryContext& context);

  SamplingResult shouldSample(const absl::optional<SpanContext> parent_context,
                              const std::string& trace_id, const std::string& name,
                              OTelSpanKind spankind,
                              OptRef<const Tracing::TraceContext> trace_context,
                              const std::vector<SpanContext>& links) override;

  std::string getDescription() const override;

private:
  std::string tenant_id_;
  std::string cluster_id_;
  std::string dt_tracestate_key_;
  SamplerConfigFetcher sampler_config_fetcher_;
  std::atomic<uint32_t> counter_; // request counter for dummy sampling
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
