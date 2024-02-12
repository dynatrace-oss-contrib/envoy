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

  explicit SamplerConfig(uint32_t default_root_spans_per_minute)
      : default_root_spans_per_minute_(default_root_spans_per_minute > 0
                                           ? default_root_spans_per_minute
                                           : ROOT_SPANS_PER_MINUTE_DEFAULT),
        root_spans_per_minute_(default_root_spans_per_minute_) {}

  void parse(const std::string& json);

  uint32_t getRootSpansPerMinute() const { return root_spans_per_minute_.load(); }

private:
  uint32_t default_root_spans_per_minute_;
  std::atomic<uint32_t> root_spans_per_minute_;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
