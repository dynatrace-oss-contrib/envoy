#pragma once

#include <cstdint>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"
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

  explicit SamplingState(uint32_t exponent) : exponent_(exponent){};

  SamplingState() = default;

  bool shouldSample(const uint64_t random_nr) const { return (random_nr % getMultiplicity() == 0); }

private:
  uint32_t exponent_{0};
};

using StreamSummaryT = StreamSummary<std::string>;
using TopKListT = std::list<Counter<std::string>>;

class SamplingController : public Logger::Loggable<Logger::Id::tracing> {

public:
  explicit SamplingController(SamplerConfigFetcherPtr sampler_config_fetcher)
      : stream_summary_(std::make_unique<StreamSummaryT>(STREAM_SUMMARY_SIZE)),
        sampler_config_fetcher_(std::move(sampler_config_fetcher)) {}

  void update();

  SamplingState getSamplingState(const std::string& sampling_key) const;

  uint64_t getEffectiveCount() const;

  void offer(const std::string& sampling_key);

  static std::string getSamplingKey(const absl::string_view path_query,
                                    const absl::string_view method);

  static constexpr size_t STREAM_SUMMARY_SIZE{100};

private:
  using SamplingExponentsT = absl::flat_hash_map<std::string, SamplingState>;
  SamplingExponentsT sampling_exponents_;
  mutable absl::Mutex sampling_exponents_mutex_{};
  std::string rest_bucket_key_{};
  static constexpr uint32_t MAX_SAMPLING_EXPONENT = (1 << 4) - 1; // 15
  std::unique_ptr<StreamSummaryT> stream_summary_;
  uint64_t last_effective_count_{};
  mutable absl::Mutex stream_summary_mutex_{};
  SamplerConfigFetcherPtr sampler_config_fetcher_;

  void logSamplingInfo(const TopKListT& top_k, const SamplingExponentsT& new_sampling_exponents,
                       uint64_t last_period_count, const uint32_t total_wanted) const;

  static uint64_t calculateEffectiveCount(const TopKListT& top_k,
                                          const SamplingExponentsT& sampling_exponents);

  void calculateSamplingExponents(const TopKListT& top_k, const uint32_t total_wanted,
                                  SamplingExponentsT& new_sampling_exponents) const;

  void update(const TopKListT& top_k, uint64_t last_period_count, const uint32_t total_wanted);
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
