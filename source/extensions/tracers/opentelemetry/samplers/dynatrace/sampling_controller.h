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
  void update(const std::list<Counter<std::string>>& top_k, uint64_t last_period_count,
              const uint32_t total_wanted) {

    // TODO: remove parameter
    (void)last_period_count;
    absl::flat_hash_map<std::string, SamplingState> new_sampling_exponents;
    // start with sampling exponent 0, which means multiplicity == 1 (every span is sampled)
    for (auto const& counter : top_k) {
      new_sampling_exponents[counter.getItem()] = {};
    }

    // use the last entry as "rest bucket", which is used for new/unknown requests
    rest_bucket_key_ = (top_k.size() > 0) ? top_k.back().getItem() : "";

    calculateSamplingExponents(top_k, total_wanted, new_sampling_exponents);

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

  uint64_t getEffectiveCount(const std::list<Counter<std::string>>& top_k) {
    absl::ReaderMutexLock lock{&mutex_};
    return computeEffectiveCount(top_k, sampling_exponents_);
  }

private:
  absl::flat_hash_map<std::string, SamplingState> sampling_exponents_;
  std::string rest_bucket_key_{};
  mutable absl::Mutex mutex_{};
  static constexpr uint32_t MAX_EXPONENT = (1 << 4) - 1; // 15

  static uint64_t
  computeEffectiveCount(const std::list<Counter<std::string>>& top_k,
                        const absl::flat_hash_map<std::string, SamplingState>& sampling_exponents) {
    uint64_t cnt = 0;
    for (auto const& counter : top_k) {
      auto sampling_state = sampling_exponents.find(counter.getItem());
      if (sampling_state == sampling_exponents.end()) {
        continue;
      }
      auto counterVal = counter.getValue();
      auto mul = sampling_state->second.getMultiplicity();
      auto res = counterVal / mul;
      cnt += res;
    }
    return cnt;
  }

  void calculateSamplingExponents(
      const std::list<Counter<std::string>>& top_k, const uint32_t total_wanted,
      absl::flat_hash_map<std::string, SamplingState>& new_sampling_exponents) {
    const auto top_k_size = top_k.size();
    if (top_k_size == 0 || total_wanted == 0) {
      return;
    }

    // number of requests which are allowed for every entry
    const uint32_t allowed_per_entry = total_wanted / top_k_size; // requests which are allowed

    for (auto& counter : top_k) {
      // allowed multiplicity for this entry
      auto wanted_multiplicity = counter.getValue() / allowed_per_entry;
      if (wanted_multiplicity < 0) {
        wanted_multiplicity = 1;
      }
      auto sampling_state = new_sampling_exponents.find(counter.getItem());
      // sampling exponent has to be a power of 2. Find the exponent to have multiplicity near to
      // wanted_multiplicity
      while (wanted_multiplicity > sampling_state->second.getMultiplicity() &&
             sampling_state->second.getExponent() < MAX_EXPONENT) {
        sampling_state->second.increaseExponent();
      }
      if (wanted_multiplicity < sampling_state->second.getMultiplicity()) {
        // we want to have multiplicity <= wanted_multiplicity, therefore exponent is decrease once.
        sampling_state->second.decreaseExponent();
      }
    }

    auto effective_count = computeEffectiveCount(top_k, new_sampling_exponents);
    // There might be entries where allowed_per_entry is greater than their count.
    // Therefore, we would sample nubmer of total_wanted requests
    // To avoid this, we decrease the exponent of other entries if possible
    if (effective_count < total_wanted) {
      for (int i = 0; i < 5; i++) { // max tries
        for (auto reverse_it = top_k.rbegin(); reverse_it != top_k.rend();
             ++reverse_it) { // start with lowest frequency
          auto rev_sampling_state = new_sampling_exponents.find(reverse_it->getItem());
          rev_sampling_state->second.decreaseExponent();
          effective_count = computeEffectiveCount(top_k, new_sampling_exponents);
          if (effective_count >= total_wanted) { // we are done
            return;
          }
        }
      }
    }
  }
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
