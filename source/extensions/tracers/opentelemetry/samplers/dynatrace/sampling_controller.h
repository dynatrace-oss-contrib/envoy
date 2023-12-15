#pragma once

#include <cstdint>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

struct SamplingState {
  uint32_t exponent_;

  [[nodiscard]] static uint32_t toMultiplicity(uint32_t exponent) { return 1 << exponent; }
  [[nodiscard]] uint32_t getExponent() const { return exponent_; }
  [[nodiscard]] uint32_t getMultiplicity() const { return toMultiplicity(exponent_); }
  void increaseExponent() { exponent_++; }
};

class SamplingController {

public:
  uint64_t
  computeEffectiveCount(const std::list<Counter<std::string>>& top_k,
                        const absl::flat_hash_map<std::string, SamplingState>& sampling_exponents) {
    uint64_t cnt = 0;
    for (auto const& counter : top_k) {
      auto sampling_state = sampling_exponents.find(counter.getItem());
      if (sampling_state == sampling_exponents.end()) {
        continue;
      }
      cnt += counter.getValue() / sampling_state->second.getMultiplicity();
    }
    return cnt;
  }

  void update(const std::list<Counter<std::string>>& top_k, const uint64_t total_requests,
              const uint64_t total_wanted) {
    (void)total_requests;
    (void)total_wanted;

    sampling_exponents_.clear();
    // const double top_k_size = top_k.size();
    for (auto const& counter : top_k) {
      sampling_exponents_[counter.getItem()] = {0};
    }
    auto effect_count = computeEffectiveCount(top_k, sampling_exponents_);

    while (effect_count > total_wanted) {
      for (auto const& counter : top_k) {
        auto sampling_state = sampling_exponents_.find(counter.getItem());
        sampling_state->second.increaseExponent();
        effect_count = computeEffectiveCount(top_k, sampling_exponents_);
        if (effect_count < total_wanted) {
          break;
        }
      }
    }
  }

  bool shouldSample(const std::string& sampling_key, const std::string& /*trace_id*/) {
    auto iter = sampling_exponents_.find(sampling_key);
    if (iter == sampling_exponents_.end()) {
      return true;
    }
    return false;
  }

  absl::flat_hash_map<std::string, SamplingState> getSamplingExponents() {
    return sampling_exponents_;
  }

private:
  absl::flat_hash_map<std::string, SamplingState> sampling_exponents_;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
