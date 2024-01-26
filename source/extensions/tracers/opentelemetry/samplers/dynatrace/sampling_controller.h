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

using StreamSummaryT = StreamSummary<std::string>;
using TopKListT = std::list<Counter<std::string>>;

class SamplingController : public Logger::Loggable<Logger::Id::tracing> {

public:
  void update(const TopKListT& top_k, uint64_t last_period_count, const uint32_t total_wanted);

  SamplingState getSamplingState(const std::string& sampling_key) const;

  uint64_t getEffectiveCount(const TopKListT& top_k) const;

  static std::string getSamplingKey(const absl::string_view path_query,
                                    const absl::string_view method);

private:
  using SamplingExponentsT = absl::flat_hash_map<std::string, SamplingState>;
  SamplingExponentsT sampling_exponents_;
  std::string rest_bucket_key_{};
  mutable absl::Mutex mutex_{};
  static constexpr uint32_t MAX_EXPONENT = (1 << 4) - 1; // 15

  void logSamplingInfo(const TopKListT& top_k, const SamplingExponentsT& new_sampling_exponents,
                       uint64_t last_period_count, const uint32_t total_wanted) const;

  uint64_t calculateEffectiveCount(const TopKListT& top_k,
                                   const SamplingExponentsT& sampling_exponents) const;

  void calculateSamplingExponents(const TopKListT& top_k, const uint32_t total_wanted,
                                  SamplingExponentsT& new_sampling_exponents) const;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
