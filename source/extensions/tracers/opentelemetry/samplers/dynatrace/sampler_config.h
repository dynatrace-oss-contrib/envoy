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
  static constexpr uint32_t ROOT_SPANS_PER_MINUTE_DEFAULT = 1000;

  void parse(const std::string& json) {
    const auto result = Envoy::Json::Factory::loadFromStringNoThrow(json);
    if (result.ok()) {
      const auto obj = result.value();
      if (obj->hasObject("rootSpansPerMinute")) {
        const auto value = obj->getInteger("rootSpansPerMinute", ROOT_SPANS_PER_MINUTE_DEFAULT);
        root_spans_per_minute_.store(value);
        return;
      }
      (void)obj;
    }
    // didn't get a value, reset to default
    root_spans_per_minute_.store(ROOT_SPANS_PER_MINUTE_DEFAULT);
  }

  uint32_t getRootSpansPerMinute() const { return root_spans_per_minute_.load(); }

private:
  std::atomic<uint32_t> root_spans_per_minute_ = ROOT_SPANS_PER_MINUTE_DEFAULT;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
