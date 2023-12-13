#include <cstdint>
#include <memory>
#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(StreamSummaryTest, TestEmpty) {
  StreamSummary<uint64_t> summary(4);
  EXPECT_EQ(summary.getN(), 0);
  auto topK = summary.getTopK();
  EXPECT_EQ(topK.size(), 0);
  EXPECT_EQ(topK.begin(), topK.end());
  EXPECT_TRUE(summary.validate().ok());
}

TEST(StreamSummaryTest, TestSimple) {
  StreamSummary<uint64_t> summary(4);
  summary.offer(4);
  summary.offer(4);
  summary.offer(4);
  summary.offer(3);
  summary.offer(3);
  summary.offer(4);
  summary.offer(1);
  summary.offer(2);

  EXPECT_TRUE(summary.validate().ok());
  EXPECT_EQ(summary.getN(), 8);

  auto topK = summary.getTopK();
  EXPECT_EQ(topK.size(), 4);
  auto it = topK.begin();
  EXPECT_EQ(it->getValue(), 4);
  EXPECT_EQ(it->getItem(), 4);
  it++;
  EXPECT_EQ(it->getValue(), 2);
  EXPECT_EQ(it->getItem(), 3);
  it++;
  EXPECT_EQ(it->getValue(), 1);
  EXPECT_EQ(it->getItem(), 1);
  it++;
  EXPECT_EQ(it->getValue(), 1);
  EXPECT_EQ(it->getItem(), 2);
}

template <typename T> std::string toString(T const& list) {
  std::ostringstream oss;
  for (auto const& counter : list) {
    oss << counter.getItem() << "(" << counter.getValue() << ")"
        << " ";
  }
  return oss.str();
}

TEST(StreamSummaryTest, TestExtendCapacity) {
  StreamSummary<uint64_t> summary(3);
  summary.offer(0);
  EXPECT_TRUE(summary.validate().ok());
  summary.offer(1);
  summary.offer(4);
  summary.offer(4);
  summary.offer(4);
  summary.offer(4);
  summary.offer(3);
  summary.offer(3);
  summary.offer(3);
  summary.offer(2);
  summary.offer(2);
  EXPECT_TRUE(summary.validate().ok());

  auto topK = summary.getTopK();
  EXPECT_EQ(topK.size(), 3);
  std::cout << __LINE__ << ": " << toString(summary.getTopK()) << std::endl;
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
