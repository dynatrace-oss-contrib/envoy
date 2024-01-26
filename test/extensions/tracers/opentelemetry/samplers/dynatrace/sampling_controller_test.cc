#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampling_controller.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

void offerEntry(StreamSummaryT& summary, const std::string& value, int count) {
  for (int i = 0; i < count; i++) {
    summary.offer(value);
  }
}

} // namespace

template <typename T> std::string toString(T const& list) {
  std::ostringstream oss;
  for (auto const& counter : list) {
    oss << counter.getItem() << ":(" << counter.getValue() << "/" << counter.getError() << ")"
        << std::endl;
  }
  return oss.str();
}

class SamplingControllerTest : public testing::Test {};

TEST_F(SamplingControllerTest, TestManyDifferentRequests) {
  StreamSummaryT summary(DynatraceSampler::STREAM_SUMMARY_SIZE);
  offerEntry(summary, "1", 2000);
  offerEntry(summary, "2", 1000);
  offerEntry(summary, "3", 750);
  offerEntry(summary, "4", 100);
  offerEntry(summary, "5", 50);
  for (int64_t i = 0; i < 2100; i++) {
    summary.offer(std::to_string(i + 1000000));
  }

  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 1000);

  // std::cout << toString(summary.getTopK());

  EXPECT_EQ(sc.getEffectiveCount(summary.getTopK()), 1110);
  EXPECT_EQ(sc.getSamplingState("1").getMultiplicity(), 128);
  EXPECT_EQ(sc.getSamplingState("2").getMultiplicity(), 64);
  EXPECT_EQ(sc.getSamplingState("3").getMultiplicity(), 64);
  EXPECT_EQ(sc.getSamplingState("4").getMultiplicity(), 8);
  EXPECT_EQ(sc.getSamplingState("5").getMultiplicity(), 4);
  EXPECT_EQ(sc.getSamplingState("1000000").getMultiplicity(), 2);
  EXPECT_EQ(sc.getSamplingState("1000001").getMultiplicity(), 2);
  EXPECT_EQ(sc.getSamplingState("1000002").getMultiplicity(), 2);
}

TEST_F(SamplingControllerTest, TestManyRequests) {
  StreamSummaryT summary(DynatraceSampler::STREAM_SUMMARY_SIZE);
  offerEntry(summary, "1", 8600);
  offerEntry(summary, "2", 5000);
  offerEntry(summary, "3", 4000);
  offerEntry(summary, "4", 4000);
  offerEntry(summary, "5", 3000);
  offerEntry(summary, "6", 30);
  offerEntry(summary, "7", 3);
  offerEntry(summary, "8", 1);

  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 1000);

  // std::cout << toString(summary.getTopK());

  EXPECT_EQ(sc.getEffectiveCount(summary.getTopK()), 1074);
  EXPECT_EQ(sc.getSamplingState("1").getMultiplicity(), 64);
  EXPECT_EQ(sc.getSamplingState("2").getMultiplicity(), 32);
  EXPECT_EQ(sc.getSamplingState("3").getMultiplicity(), 32);
  EXPECT_EQ(sc.getSamplingState("4").getMultiplicity(), 16);
  EXPECT_EQ(sc.getSamplingState("5").getMultiplicity(), 8);
  EXPECT_EQ(sc.getSamplingState("6").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("7").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("8").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestSomeRequests) {
  StreamSummaryT summary(DynatraceSampler::STREAM_SUMMARY_SIZE);
  offerEntry(summary, "1", 7500);
  offerEntry(summary, "2", 1000);
  offerEntry(summary, "3", 1);
  offerEntry(summary, "4", 1);
  offerEntry(summary, "5", 1);
  for (int64_t i = 0; i < 11; i++) {
    summary.offer(std::to_string(i + 1000000));
  }

  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 1000);

  EXPECT_EQ(sc.getEffectiveCount(summary.getTopK()), 1451);
  EXPECT_EQ(sc.getSamplingState("1").getMultiplicity(), 8);
  EXPECT_EQ(sc.getSamplingState("2").getMultiplicity(), 2);
  EXPECT_EQ(sc.getSamplingState("3").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("4").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("5").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("1000000").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("1000001").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("1000002").getMultiplicity(), 1);
  EXPECT_EQ(sc.getSamplingState("1000003").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestSimple) {
  StreamSummaryT summary(10);
  // offerEntry data
  offerEntry(summary, "GET_xxxx", 300);
  offerEntry(summary, "POST_asdf", 200);
  offerEntry(summary, "GET_asdf", 100);

  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 100);

  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getExponent(), 3);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getMultiplicity(), 8);

  EXPECT_EQ(sc.getSamplingState("POST_asdf").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getMultiplicity(), 4);

  EXPECT_EQ(sc.getSamplingState("GET_asdf").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getMultiplicity(), 2);

  // total_wanted > number of requests
  sc.update(summary.getTopK(), summary.getN(), 1000);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getMultiplicity(), 1);

  EXPECT_EQ(sc.getSamplingState("POST_asdf").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getMultiplicity(), 1);

  EXPECT_EQ(sc.getSamplingState("GET_asdf").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestEmpty) {
  StreamSummaryT summary(10);
  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 100);

  EXPECT_EQ(sc.getSamplingState("GET_something").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_something").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestNonExisting) {
  StreamSummaryT summary(10);
  summary.offer("key1");
  SamplingController sc;
  sc.update(summary.getTopK(), summary.getN(), 100);

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

TEST_F(SamplingControllerTest, TestGetSamplingKey) {
  std::string key = SamplingController::getSamplingKey("somepath", "GET");
  EXPECT_STREQ(key.c_str(), "GET_somepath");

  key = SamplingController::getSamplingKey("somepath?withquery", "POST");
  EXPECT_STREQ(key.c_str(), "POST_somepath");

  key = SamplingController::getSamplingKey("anotherpath", "PUT");
  EXPECT_STREQ(key.c_str(), "PUT_anotherpath");
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
