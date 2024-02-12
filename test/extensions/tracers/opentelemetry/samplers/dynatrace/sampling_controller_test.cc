#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampling_controller.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

void offerEntry(SamplingController& sc, const std::string& value, int count) {
  for (int i = 0; i < count; i++) {
    sc.offer(value);
  }
}

} // namespace

class TestSamplerConfigFetcher : public SamplerConfigFetcher {
public:
  TestSamplerConfigFetcher(
      uint32_t root_spans_per_minute = SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT)
      : config(root_spans_per_minute) {}
  const SamplerConfig& getSamplerConfig() const override { return config; }
  SamplerConfig config;
};

class SamplingControllerTest : public testing::Test {};

TEST_F(SamplingControllerTest, TestManyDifferentRequests) {
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  offerEntry(sc, "1", 2000);
  offerEntry(sc, "2", 1000);
  offerEntry(sc, "3", 750);
  offerEntry(sc, "4", 100);
  offerEntry(sc, "5", 50);
  for (int64_t i = 0; i < 2100; i++) {
    sc.offer(std::to_string(i + 1000000));
  }

  sc.update();

  EXPECT_EQ(sc.getEffectiveCount(), 1110);
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
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  offerEntry(sc, "1", 8600);
  offerEntry(sc, "2", 5000);
  offerEntry(sc, "3", 4000);
  offerEntry(sc, "4", 4000);
  offerEntry(sc, "5", 3000);
  offerEntry(sc, "6", 30);
  offerEntry(sc, "7", 3);
  offerEntry(sc, "8", 1);

  sc.update();

  EXPECT_EQ(sc.getEffectiveCount(), 1074);
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
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  offerEntry(sc, "1", 7500);
  offerEntry(sc, "2", 1000);
  offerEntry(sc, "3", 1);
  offerEntry(sc, "4", 1);
  offerEntry(sc, "5", 1);
  for (int64_t i = 0; i < 11; i++) {
    sc.offer(std::to_string(i + 1000000));
  }

  sc.update();

  EXPECT_EQ(sc.getEffectiveCount(), 1451);
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
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  scf->config.parse("{\n \"rootSpansPerMinute\" : 100 \n }");
  SamplingController sc(std::move(scf));

  offerEntry(sc, "GET_xxxx", 300);
  offerEntry(sc, "POST_asdf", 200);
  offerEntry(sc, "GET_asdf", 100);

  sc.update();

  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getExponent(), 3);
  EXPECT_EQ(sc.getSamplingState("GET_xxxx").getMultiplicity(), 8);

  EXPECT_EQ(sc.getSamplingState("POST_asdf").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("POST_asdf").getMultiplicity(), 4);

  EXPECT_EQ(sc.getSamplingState("GET_asdf").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_asdf").getMultiplicity(), 2);
}

TEST_F(SamplingControllerTest, TestWarmup) {
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  // offer entries, but don't call update();
  // sampling exponents table will be empty
  // exponent will be calculated based on count.
  offerEntry(sc, "GET_0", 10);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_2").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_3").getExponent(), 0);

  offerEntry(sc, "GET_1", 540);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_2").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_3").getExponent(), 1);

  offerEntry(sc, "GET_0", 300);
  EXPECT_EQ(sc.getSamplingState("GET_0").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 1);
  EXPECT_EQ(sc.getSamplingState("GET_10").getExponent(), 1);

  offerEntry(sc, "GET_4", 550);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("GET_2").getExponent(), 2);
  EXPECT_EQ(sc.getSamplingState("GET_3").getExponent(), 2);

  offerEntry(sc, "GET_5", 1000);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 4);
  EXPECT_EQ(sc.getSamplingState("GET_2").getExponent(), 4);
  EXPECT_EQ(sc.getSamplingState("GET_3").getExponent(), 4);

  offerEntry(sc, "GET_7", 2000);
  EXPECT_EQ(sc.getSamplingState("GET_1").getExponent(), 8);
  EXPECT_EQ(sc.getSamplingState("GET_2").getExponent(), 8);
  EXPECT_EQ(sc.getSamplingState("GET_3").getExponent(), 8);
}

TEST_F(SamplingControllerTest, TestEmpty) {
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  sc.update();

  EXPECT_EQ(sc.getSamplingState("GET_something").getExponent(), 0);
  EXPECT_EQ(sc.getSamplingState("GET_something").getMultiplicity(), 1);
}

TEST_F(SamplingControllerTest, TestNonExisting) {
  auto scf = std::make_unique<TestSamplerConfigFetcher>();
  SamplingController sc(std::move(scf));

  sc.offer("key1");
  sc.update();

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
