#pragma once

#include <memory>

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"
#include "envoy/server/factory_context.h"

#include "source/common/common/logger.h"
#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampling_controller.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"

#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class FW4Tag {
public:
  static FW4Tag createInvalid() { return {false, false, 0, 0}; }

  static FW4Tag create(bool ignored, uint32_t sampling_exponent, uint32_t path_info) {
    return {true, ignored, sampling_exponent, path_info};
  }

  static FW4Tag create(const std::string& value) {
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
  FW4Tag(bool valid, bool ignored, uint32_t sampling_exponent, uint32_t path_info)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent),
        path_info_(path_info) {}

  bool valid_;
  bool ignored_;
  uint32_t sampling_exponent_;
  uint32_t path_info_;
};

/**
 * @brief A Dynatrace specific sampler
 */
class DynatraceSampler : public Sampler, Logger::Loggable<Logger::Id::tracing> {
public:
  DynatraceSampler(
      const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config,
      Server::Configuration::TracerFactoryContext& context,
      SamplerConfigFetcherPtr sampler_config_fetcher);

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
  Event::TimerPtr timer_;
  SamplingController sampling_controller_;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
