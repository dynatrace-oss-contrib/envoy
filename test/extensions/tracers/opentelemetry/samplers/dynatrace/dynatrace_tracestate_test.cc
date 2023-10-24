#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(DynatraceTracestateTest, TestKey) {
  DynatraceTracestate tracestate("98812ad49", "980df25c");
  EXPECT_STREQ(tracestate.getKey().c_str(), "98812ad49-980df25c@dt");
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
