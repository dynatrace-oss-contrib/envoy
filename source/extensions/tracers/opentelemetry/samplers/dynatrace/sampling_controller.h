#pragma once

#include <cstdint>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplingState {
public:
  [[nodiscard]] static uint32_t toMultiplicity(uint32_t exponent) { return 1 << exponent; }
  [[nodiscard]] uint32_t getExponent() const { return exponent_; }
  [[nodiscard]] uint32_t getMultiplicity() const { return toMultiplicity(exponent_); }
  void increaseExponent() { exponent_++; }
  void decreaseExponent() {
    if (exponent_ > 0) {
      exponent_--;
    }
  }

  bool shouldSample(const uint64_t random_nr) const { return (random_nr % getMultiplicity() == 0); }

private:
  uint32_t exponent_{};
};

class SamplingController {

public:
  void update(const std::list<Counter<std::string>>& top_k, const uint32_t total_wanted) {

    absl::flat_hash_map<std::string, SamplingState> new_sampling_exponents;
    // start with sampling exponent 0, which means multiplicity == 1 (every span is sampled)
    for (auto const& counter : top_k) {
      new_sampling_exponents[counter.getItem()] = {};
    }

    // use the last entry as "rest bucket", which is used for new/unknown requests
    rest_bucket_key_ = (top_k.size() > 0) ? top_k.back().getItem() : "";

    auto effective_count = computeEffectiveCount(top_k, new_sampling_exponents);

    while (effective_count > total_wanted) {
      for (auto const& counter : top_k) {
        auto sampling_state = new_sampling_exponents.find(counter.getItem());
        sampling_state->second.increaseExponent();
        effective_count = computeEffectiveCount(top_k, new_sampling_exponents);
        if (effective_count <= total_wanted) {
          // we want to be close to total_wanted, but we don't want less than total_wanted samples.
          // Therefore, we decrease the exponent again to keep effective_count > total_wanted
          sampling_state->second.decreaseExponent();
          break;
        }
      }
    }
    absl::WriterMutexLock lock{&mutex_};
    sampling_exponents_ = std::move(new_sampling_exponents);
  }

  SamplingState getSamplingState(const std::string& sampling_key) const {
    absl::ReaderMutexLock lock{&mutex_};
    auto iter = sampling_exponents_.find(sampling_key);
    if (iter ==
        sampling_exponents_
            .end()) { // we don't have a sampling exponent for this exponent, use "rest bucket"
      auto rest_bucket_iter = sampling_exponents_.find(rest_bucket_key_);
      if (rest_bucket_iter != sampling_exponents_.end()) {
        return rest_bucket_iter->second;
      }
      return {};
    }
    return iter->second;
  }

private:
  absl::flat_hash_map<std::string, SamplingState> sampling_exponents_;
  std::string rest_bucket_key_{};
  mutable absl::Mutex mutex_{};

  static uint64_t
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
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
