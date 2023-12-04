#include <memory>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

// Test sampler config json parsing
TEST(SamplerConfigTest, test) {
  SamplerConfig config;
  config.parse("{\n \"rootSpansPerMinute\" : 2000 \n }");
  EXPECT_EQ(config.getRootSpansPerMinute(), 2000u);
  config.parse("{\n \"rootSpansPerMinute\" : 10000 \n }");
  EXPECT_EQ(config.getRootSpansPerMinute(), 10000u);

  // unexpected json, default value should be used
  config.parse("{}");
  EXPECT_EQ(config.getRootSpansPerMinute(), SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);

  config.parse("");
  EXPECT_EQ(config.getRootSpansPerMinute(), SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);

  config.parse("\\");
  EXPECT_EQ(config.getRootSpansPerMinute(), SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);

  config.parse(" { ");
  EXPECT_EQ(config.getRootSpansPerMinute(), SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);

  config.parse(" } ");
  EXPECT_EQ(config.getRootSpansPerMinute(), SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
