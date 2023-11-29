#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

#include "source/common/json/json_loader.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplerConfig {
public:
  static constexpr uint64_t DEFAULT_THRESHOLD = 1000;

  void parse(const std::string& json) {
    root_sampler_path_per_minute_threshold.store(DEFAULT_THRESHOLD); // reset to default
    auto result = Envoy::Json::Factory::loadFromStringNoThrow(json);
    if (result.ok()) {
      auto obj = result.value();
      if (obj->hasObject("threshold")) {
        auto value = obj->getInteger("threshold", DEFAULT_THRESHOLD);
        root_sampler_path_per_minute_threshold.store(value);
      }
      (void)obj;
    }
  }

  uint64_t getThreshHold() const { return root_sampler_path_per_minute_threshold.load(); }

private:
  std::atomic<uint64_t> root_sampler_path_per_minute_threshold = DEFAULT_THRESHOLD;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
