#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplerConfig {
public:
  static constexpr uint32_t ROOT_SPANS_PER_MINUTE_DEFAULT = 1000;

  void parse(const std::string& json);

  uint32_t getRootSpansPerMinute() const { return root_spans_per_minute_.load(); }

private:
  std::atomic<uint32_t> root_spans_per_minute_ = ROOT_SPANS_PER_MINUTE_DEFAULT;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
