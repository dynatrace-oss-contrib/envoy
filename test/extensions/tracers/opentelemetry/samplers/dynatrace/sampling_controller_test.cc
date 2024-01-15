#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
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

  EXPECT_EQ(sc.getSamplingState("GET_asdf").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getMultiplicity(), 4);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getMultiplicity(), 4);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getExponent(), 3);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getMultiplicity(), 8);

  // total_wanted > number of requests
  sc.update(summary.getTopK(), 1000);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestEmpty) {
  StreamSummary<std::string> summary(10);
  SamplingController sc;
  sc.update(summary.getTopK(), 100);

  EXPECT_EQ(sc.getSamplingState("GET_something").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_something").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestNonExisting) {
  StreamSummary<std::string> summary(10);
  summary.offer("key1");
  SamplingController sc;
  sc.update(summary.getTopK(), 100);

  EXPECT_EQ(sc.getSamplingState("key2").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("key2").getMultiplicity(), 1);
}

TEST(SamplingStateTest, TestIncreaseDecrease) {
  SamplingState sst{};
  EXPECT_EQ(sst.getExponent(), 0);
  EXPECT_EQ(sst.getMultiplicity(), 1);

  sst.increaseExponent();
  EXPECT_EQ(sst.getExponent(), 1);
  EXPECT_EQ(sst.getMultiplicity(), 2);

  sst.increaseExponent();
  EXPECT_EQ(sst.getExponent(), 2);
  EXPECT_EQ(sst.getMultiplicity(), 4);

  for (int i = 0; i < 6; i++) {
    sst.increaseExponent();
  }
  EXPECT_EQ(sst.getExponent(), 8);
  EXPECT_EQ(sst.getMultiplicity(), 256);

  sst.decreaseExponent();
  EXPECT_EQ(sst.getExponent(), 7);
  EXPECT_EQ(sst.getMultiplicity(), 128);
}

TEST(SamplingStateTest, TestShouldSample) {
  // default sampling state should sample
  SamplingState sst{};
  EXPECT_TRUE(sst.shouldSample(1234));
  EXPECT_TRUE(sst.shouldSample(3456));
  EXPECT_TRUE(sst.shouldSample(12345));

  // exponent 2, multiplicity 1, even (=not odd) random numbers should be sampled
  sst.increaseExponent();
  EXPECT_TRUE(sst.shouldSample(22));
  EXPECT_TRUE(sst.shouldSample(4444444));
  EXPECT_FALSE(sst.shouldSample(21));
  EXPECT_FALSE(sst.shouldSample(111111));

  for (int i = 0; i < 9; i++) {
    sst.increaseExponent();
  }
  // exponent 10, multiplicity 1024,
  EXPECT_TRUE(sst.shouldSample(1024));
  EXPECT_TRUE(sst.shouldSample(2048));
  EXPECT_TRUE(sst.shouldSample(4096));
  EXPECT_TRUE(sst.shouldSample(10240000000));
  EXPECT_FALSE(sst.shouldSample(1023));
  EXPECT_FALSE(sst.shouldSample(1025));
  EXPECT_FALSE(sst.shouldSample(2047));
  EXPECT_FALSE(sst.shouldSample(2049));
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
