#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampling_controller.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplingControllerTest : public testing::Test {

protected:
  std::vector<std::string> getRequests() {
    std::vector<std::string> requests{};
    for (int i = 0; i < 100; i++) {
      requests.push_back("GET_asdf");
    }
    for (int i = 0; i < 200; i++) {
      requests.push_back("POST_asdf");
    }
    for (int i = 0; i < 300; i++) {
      requests.push_back("GET_xxxx");
    }
    return requests;
  }
};

TEST_F(SamplingControllerTest, TestSimple) {
  std::vector<std::string> requests = getRequests();
  StreamSummary<std::string> summary(10);
  for (auto const& c : requests) {
    summary.offer(c);
  }
  SamplingController sc;
  sc.update(summary.getTopK(), 100);

  auto ex = sc.getSamplingExponents();
  EXPECT_EQ(ex["GET_asdf"].getExponent(), 2);
  EXPECT_EQ(ex["GET_asdf"].getMultiplicity(), 4);
  EXPECT_EQ(ex["POST_asdf"].getExponent(), 2);
  EXPECT_EQ(ex["POST_asdf"].getMultiplicity(), 4);
  EXPECT_EQ(ex["GET_xxxx"].getExponent(), 3);
  EXPECT_EQ(ex["GET_xxxx"].getMultiplicity(), 8);

  // total_wanted > number of requests
  sc.update(summary.getTopK(), 1000);
  ex = sc.getSamplingExponents();
  EXPECT_EQ(ex["GET_asdf"].getExponent(), 0);
  EXPECT_EQ(ex["GET_asdf"].getMultiplicity(), 1);
  EXPECT_EQ(ex["POST_asdf"].getExponent(), 0);
  EXPECT_EQ(ex["POST_asdf"].getMultiplicity(), 1);
  EXPECT_EQ(ex["GET_xxxx"].getExponent(), 0);
  EXPECT_EQ(ex["GET_xxxx"].getMultiplicity(), 1);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
