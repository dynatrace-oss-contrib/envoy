#include <memory>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(SamplerConfigTest, test) {
  SamplerConfig config;
  config.parse("{\n \"threshold\" : 2000 \n }");
  EXPECT_EQ(config.getThreshHold(), 2000u);
  config.parse("{\n \"threshold\" : 10000 \n }");
  EXPECT_EQ(config.getThreshHold(), 10000u);

  // unexpected json, default value should be used
  config.parse("{ }");
  EXPECT_EQ(config.getThreshHold(), SamplerConfig::DEFAULT_THRESHOLD);

  config.parse("");
  EXPECT_EQ(config.getThreshHold(), SamplerConfig::DEFAULT_THRESHOLD);

  config.parse("\\");
  EXPECT_EQ(config.getThreshHold(), SamplerConfig::DEFAULT_THRESHOLD);

  config.parse(" { ");
  EXPECT_EQ(config.getThreshHold(), SamplerConfig::DEFAULT_THRESHOLD);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
