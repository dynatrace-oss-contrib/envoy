#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

FW4Tag FW4Tag::createInvalid() { return {false, false, 0}; }

FW4Tag FW4Tag::create(bool ignored, int sampling_exponent) {
  return {true, ignored, sampling_exponent};
}

FW4Tag FW4Tag::create(const std::string& value) {
  std::vector<absl::string_view> tracestate_components =
      absl::StrSplit(value, ';', absl::AllowEmpty());
  if (tracestate_components.size() < 7) {
    return createInvalid();
  }

  if (tracestate_components[0] != "fw4") {
    return createInvalid();
  }
  bool ignored = tracestate_components[5] == "1";
  int sampling_exponent = std::stoi(std::string(tracestate_components[6]));
  return {true, ignored, sampling_exponent};
}

std::string FW4Tag::asString() const {
  std::string ret =
      absl::StrCat("fw4;0;0;0;0;", ignored_ ? "1" : "0", ";", sampling_exponent_, ";0");
  return ret;
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
