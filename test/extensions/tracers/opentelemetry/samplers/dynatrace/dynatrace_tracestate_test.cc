#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(TraceStateTest, TestHashExtension) {
  auto res = DynatraceTracestate::hash_extension(";7h2947bad48fb3d7da");
  EXPECT_STREQ(res.c_str(), "68a4");
  res = DynatraceTracestate::hash_extension(";7h001b0e7bdd2c9657");
  EXPECT_STREQ(res.c_str(), "e9a7");
}

TEST(TraceStateTest, TestParse) {
  DynatraceTracestate tracestate = DynatraceTracestate::parse(
      "9712ad49-980df25b@dt=fw4;0;0;0;0;0;0;3d0;bad6;7hd9d481c5a1d12a8b");
  EXPECT_STREQ(tracestate.tenant_id.c_str(), "9712ad49");
  EXPECT_STREQ(tracestate.cluster_id.c_str(), "980df25b");
  EXPECT_STREQ(tracestate.path_info.c_str(), "3d0");
  EXPECT_STREQ(tracestate.is_ignored.c_str(), "0");
  // TODO: we should strip 7h?
  EXPECT_STREQ(tracestate.span_id.c_str(), "7hd9d481c5a1d12a8b");
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
