#pragma once

#include <algorithm>
#include <cstdint>
#include <list>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"

// port of https://github.com/fzakaria/space-saving/tree/master

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace detail {

template <typename T> struct Bucket;

template <typename T> using BucketIterator = typename std::list<Bucket<T>>::iterator;

template <typename T> struct Counter {
  BucketIterator<T> bucket;
  absl::optional<T> item{};
  uint64_t value{};
  uint64_t error{};

  explicit Counter(BucketIterator<T> bucket) : bucket(bucket) {}
  Counter(Counter const&) = delete;
  Counter& operator=(Counter const&) = delete;
};

template <typename T> using CounterIterator = typename std::list<Counter<T>>::iterator;

template <typename T> struct Bucket {
  uint64_t value;
  std::list<Counter<T>> children{};

  explicit Bucket(uint64_t value) : value(value) {}
  Bucket(Bucket const&) = delete;
  Bucket& operator=(Bucket const&) = delete;
};

} // namespace detail

template <typename T> class Counter {
private:
  T const& item_;
  uint64_t value_;
  uint64_t error_;

public:
  Counter(detail::Counter<T> const& c) : item_(*c.item), value_(c.value), error_(c.error) {}

  T const& getItem() const { return item_; }
  uint64_t getValue() const { return value_; }
  uint64_t getError() const { return error_; }
};

template <typename T> class StreamSummary {
private:
  const size_t capacity_;
  uint64_t n_{};
  absl::flat_hash_map<T, detail::CounterIterator<T>> cache_{};
  std::list<detail::Bucket<T>> buckets_{};

  typename detail::CounterIterator<T> incrementCounter(detail::CounterIterator<T> counter_iter,
                                                       const uint64_t increment) {
    auto const bucket = counter_iter->bucket;
    auto bucketNext = std::prev(bucket);
    counter_iter->value += increment;

    detail::CounterIterator<T> elem;
    if (bucketNext != buckets_.end() && counter_iter->value == bucketNext->value) {
      counter_iter->bucket = bucketNext;
      bucketNext->children.splice(bucketNext->children.end(), bucket->children, counter_iter);
      elem = std::prev(bucketNext->children.end());
      // validate();
    } else {
      auto bucketNew = buckets_.emplace(bucket, counter_iter->value);
      counter_iter->bucket = bucketNew;
      bucketNew->children.splice(bucketNew->children.end(), bucket->children, counter_iter);
      elem = std::prev(bucketNew->children.end());
      // validate();
    }
    if (bucket->children.empty()) {
      buckets_.erase(bucket);
    }
    return elem;
  }

  absl::Status validateInternal() const {
    auto cache_copy = cache_;
    auto current_bucket = buckets_.begin();
    uint64_t value_sum = 0;
    while (current_bucket != buckets_.end()) {
      auto prev = std::prev(current_bucket);
      if (prev != buckets_.end() && prev->value <= current_bucket->value) {
        return absl::InternalError("buckets should be in descending order.");
      }
      auto current_child = current_bucket->children.begin();
      while (current_child != current_bucket->children.end()) {
        if (current_child->bucket != current_bucket) {
          return absl::InternalError("entry should point to its bucket.");
        }
        if (current_child->value != current_bucket->value) {
          return absl::InternalError("entry and bucket should have the same value.");
        }
        if (current_child->item) {
          auto old_iter = cache_copy.find(*current_child->item);
          if (old_iter != cache_copy.end()) {
            cache_copy.erase(old_iter);
          }
        }
        value_sum += current_child->value;
        current_child++;
      }
      current_bucket++;
    }
    if (!cache_copy.empty()) {
      return absl::InternalError("there should be no dead cached entries.");
    }
    if (cache_.size() > capacity_) {
      return absl::InternalError("cache size must not exceed capacity");
    }
    if (value_sum != n_) {
      return absl::InternalError("sum of all counter->value() must be equal to n");
    }
    return absl::OkStatus();
  }

public:
  explicit StreamSummary(const size_t capacity) : capacity_(capacity) {
    auto& newBucket = buckets_.emplace_back(0);
    for (size_t i = 0; i < capacity; ++i) {
      newBucket.children.emplace_back(buckets_.begin());
    }
    // validate();
  }

  size_t getCapacity() const { return capacity_; }

  absl::Status validate() const { return validateInternal(); }

  Counter<T> offer(T const& item, const uint64_t increment = 1) {
    // validate();

    ++n_;
    auto iter = cache_.find(item);
    if (iter != cache_.end()) {
      iter->second = incrementCounter(iter->second, increment);
      // validate();
      return *iter->second;
    } else {
      auto minElement = std::prev(buckets_.back().children.end());
      auto originalMinValue = minElement->value;
      if (minElement->item) {
        // remove old from cache
        auto old_iter = cache_.find(*minElement->item);
        if (old_iter != cache_.end()) {
          cache_.erase(old_iter);
        }
      }
      minElement->item = item;
      minElement = incrementCounter(minElement, increment);
      cache_[item] = minElement;
      // TODO: I think the following comment and `if (cache_.size() <= capacity_)` do not make
      // sense.
      // TODO: cache_.size() has to be <= capacity!
      // if we aren't full on capacity yet, we don't need to add error since we have seen every item
      // so far
      if (cache_.size() <= capacity_) {
        minElement->error = originalMinValue;
      }
      // validate();
      return *minElement;
    }
  }

  uint64_t getN() const { return n_; }

  typename std::list<Counter<T>> getTopK(const size_t k = SIZE_MAX) const {
    std::list<Counter<T>> r;
    for (auto const& bucket : buckets_) {
      for (auto const& child : bucket.children) {
        if (child.item) {
          r.emplace_back(child);
          if (r.size() == k) {
            return r;
          }
        }
      }
    }
    return r;
  }

  // this makes using the error somewhat useless
  void scaleDown(const float factor) {
    n_ = 0;
    for (auto bucket_iter = buckets_.begin(); bucket_iter != buckets_.end(); bucket_iter++) {
      bucket_iter->value = std::max<float>(bucket_iter->value / factor, 1.);
      for (auto& child : bucket_iter->children) {
        child.value = std::max<float>(child.value / factor, 1.);
        n_ += child.value;
        child.error /= factor;
      }
      auto prev = std::prev(bucket_iter);
      if (prev != buckets_.end() && prev->value == bucket_iter->value) {
        std::for_each(bucket_iter->children.begin(), bucket_iter->children.end(),
                      [&prev](detail::Counter<T>& c) { c.bucket = prev; });
        prev->children.splice(prev->children.end(), bucket_iter->children,
                              bucket_iter->children.begin(), bucket_iter->children.end());
        buckets_.erase(bucket_iter);
        bucket_iter = prev;
      }
    }
    // validate();
  }
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
