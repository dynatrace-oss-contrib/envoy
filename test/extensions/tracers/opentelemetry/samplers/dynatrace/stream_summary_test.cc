#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/stream_summary.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

template <typename T>
void CompareCounter(typename std::list<Counter<T>>::iterator counter, T item, uint64_t value,
                    uint64_t error, int line_num) {
  SCOPED_TRACE(absl::StrCat(__FUNCTION__, " called from line ", line_num));
  EXPECT_EQ(counter->getValue(), value);
  EXPECT_EQ(counter->getItem(), item);
  EXPECT_EQ(counter->getError(), error);
}

} // namespace

TEST(StreamSummaryTest, TestEmpty) {
  StreamSummary<std::string> summary(4);
  EXPECT_EQ(summary.getN(), 0);
  auto topK = summary.getTopK();
  EXPECT_EQ(topK.size(), 0);
  EXPECT_EQ(topK.begin(), topK.end());
  EXPECT_TRUE(summary.validate().ok());
}

TEST(StreamSummaryTest, TestSimple) {
  StreamSummary<char> summary(4);
  summary.offer('a');
  summary.offer('a');
  summary.offer('b');
  summary.offer('a');
  summary.offer('c');
  summary.offer('b');
  summary.offer('a');
  summary.offer('d');

  EXPECT_TRUE(summary.validate().ok());
  EXPECT_EQ(summary.getN(), 8);

  auto topK = summary.getTopK();
  EXPECT_EQ(topK.size(), 4);
  auto it = topK.begin();
  CompareCounter(it, 'a', 4, 0, __LINE__);
  CompareCounter(++it, 'b', 2, 0, __LINE__);
  CompareCounter(++it, 'c', 1, 0, __LINE__);
  CompareCounter(++it, 'd', 1, 0, __LINE__);
}

// TODO: remove
template <typename T> std::string toString(T const& list) {
  std::ostringstream oss;
  for (auto const& counter : list) {
    oss << counter.getItem() << ":(" << counter.getValue() << "/" << counter.getError() << ")"
        << " ";
  }
  return oss.str();
}

TEST(StreamSummaryTest, TestExtendCapacity) {
  StreamSummary<char> summary(3);
  EXPECT_TRUE(summary.validate().ok());
  summary.offer('d');
  summary.offer('a');
  summary.offer('b');
  summary.offer('a');
  summary.offer('a');
  summary.offer('a');
  summary.offer('b');
  summary.offer('c');
  summary.offer('b');
  summary.offer('c');
  EXPECT_TRUE(summary.validate().ok());

  {
    auto topK = summary.getTopK();
    auto it = topK.begin();
    EXPECT_EQ(topK.size(), 3);
    CompareCounter(it, 'a', 4, 0, __LINE__);
    CompareCounter(++it, 'b', 3, 0, __LINE__);
    CompareCounter(++it, 'c', 3, 1, __LINE__);
    std::cout << __LINE__ << ": " << toString(summary.getTopK()) << std::endl;
  }

  // add item 'e', 'c' should be removed.
  summary.offer('e');
  {
    auto topK = summary.getTopK();
    auto it = topK.begin();
    EXPECT_EQ(topK.size(), 3);
    CompareCounter(it, 'a', 4, 0, __LINE__);
    CompareCounter(++it, 'e', 4, 3, __LINE__);
    CompareCounter(++it, 'b', 3, 0, __LINE__);
  }
}

TEST(StreamSummaryTest, TestRandomInsertOrder) {
  std::vector<char> v{'a', 'a', 'a', 'a', 'a', 'a', 'b', 'b', 'b', 'b', 'b',
                      'c', 'c', 'c', 'c', 'd', 'd', 'd', 'e', 'e', 'f'};
  for (int i = 0; i < 5; ++i) {
    // insert order should not matter if all items have a different count in input stream
    std::shuffle(v.begin(), v.end(), std::default_random_engine());
    StreamSummary<char> summary(10);
    for (auto const c : v) {
      summary.offer(c);
    }
    auto topK = summary.getTopK();
    auto it = topK.begin();
    CompareCounter(it, 'a', 6, 0, __LINE__);
    CompareCounter(++it, 'b', 5, 0, __LINE__);
    CompareCounter(++it, 'c', 4, 0, __LINE__);
    CompareCounter(++it, 'd', 3, 0, __LINE__);
    CompareCounter(++it, 'e', 2, 0, __LINE__);
    CompareCounter(++it, 'f', 1, 0, __LINE__);
  }
}

TEST(StreamSummaryTest, TestGetK) {
  std::vector<char> v{'a', 'a', 'a', 'a', 'a', 'a', 'b', 'b', 'b', 'b', 'b',
                      'c', 'c', 'c', 'c', 'd', 'd', 'd', 'e', 'e', 'f'};
  std::shuffle(v.begin(), v.end(), std::default_random_engine());
  StreamSummary<char> summary(20);
  for (auto const c : v) {
    summary.offer(c);
  }
  EXPECT_EQ(summary.getTopK().size(), 6);
  EXPECT_EQ(summary.getTopK(1).size(), 1);
  EXPECT_EQ(summary.getTopK(2).size(), 2);
  EXPECT_EQ(summary.getTopK(3).size(), 3);
  EXPECT_EQ(summary.getTopK(4).size(), 4);
  EXPECT_EQ(summary.getTopK(5).size(), 5);
  EXPECT_EQ(summary.getTopK(6).size(), 6);
  EXPECT_EQ(summary.getTopK(7).size(), 6);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
