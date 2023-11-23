#pragma once

#include <string>
#include <vector>

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class TracestateEntry {
  // TODO: could be string_views to Tracestate::raw_trace_state_
public:
  std::string key;
  std::string value;
};

/**
 * @brief parses and manipultes W3C tracestate header
 * see https://www.w3.org/TR/trace-context/#tracestate-header
 *
 */
class Tracestate {
public:
  void parse(const std::string& tracestate);

  void add(const std::string& key, const std::string& value);

  std::vector<TracestateEntry> entries() const { return entries_; }

  std::string asString() const { return raw_trace_state_; }

private:
  std::vector<TracestateEntry> entries_;
  std::string raw_trace_state_;

  void clear();
};
} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
