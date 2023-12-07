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
  static constexpr uint64_t ROOT_SPANS_PER_MINUTE_DEFAULT = 1000;

  void parse(const std::string& json) {
    root_spans_per_minute_.store(ROOT_SPANS_PER_MINUTE_DEFAULT); // reset to default
    auto result = Envoy::Json::Factory::loadFromStringNoThrow(json);
    if (result.ok()) {
      auto obj = result.value();
      if (obj->hasObject("rootSpansPerMinute")) {
        auto value = obj->getInteger("rootSpansPerMinute", ROOT_SPANS_PER_MINUTE_DEFAULT);
        root_spans_per_minute_.store(value);
      }
      (void)obj;
    }
  }

  uint64_t getRootSpansPerMinute() const { return root_spans_per_minute_.load(); }

private:
  std::atomic<uint64_t> root_spans_per_minute_ = ROOT_SPANS_PER_MINUTE_DEFAULT;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
