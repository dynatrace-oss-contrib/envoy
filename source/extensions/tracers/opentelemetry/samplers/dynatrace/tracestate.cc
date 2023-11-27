#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/tracestate.h"

#include "source/common/common/utility.h"

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

void Tracestate::parse(const std::string& tracestate) {
  clear();
  raw_trace_state_ = tracestate;
  std::vector<absl::string_view> list_members = absl::StrSplit(tracestate, ',', absl::SkipEmpty());
  for (auto const& list_member : list_members) {
    std::vector<absl::string_view> kv = absl::StrSplit(list_member, '=', absl::SkipEmpty());
    if (kv.size() != 2) {
      // unexpected entry, ignore
      continue;
    }
    absl::string_view key = StringUtil::ltrim(kv[0]);
    absl::string_view val = StringUtil::rtrim(kv[1]);
    TracestateEntry entry{{key.data(), key.size()}, {val.data(), val.size()}};
    entries_.push_back(std::move(entry));
  }
}

void Tracestate::add(const std::string& key, const std::string& value) {
  std::string tracestate =
      absl::StrCat(absl::string_view(key), "=", absl::string_view(value),
                   raw_trace_state_.empty() > 0 ? "" : ",", absl::string_view(raw_trace_state_));
  parse(tracestate);
}

void Tracestate::clear() {
  entries_.clear();
  raw_trace_state_.clear();
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
