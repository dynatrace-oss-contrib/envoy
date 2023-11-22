#pragma once

#include <cstdint>
#include <sstream>
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

class FW4Tag {
public:
  static FW4Tag createInvalid() { return {false, false, 0}; }

  static FW4Tag create(bool ignored, int sampling_exponent) {
    return {true, ignored, sampling_exponent};
  }

  static FW4Tag create(const std::string& value) {
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

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  int getSamplingExponent() const { return sampling_exponent_; };

  std::string createValue() const {
    std::string ret =
        absl::StrCat("fw4;0;0;0;0;", ignored_ ? "1" : "0", ";", sampling_exponent_, ";0");
    return ret;
  }

private:
  FW4Tag(bool valid, bool ignored, int sampling_exponent)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent) {}

  bool valid_;
  bool ignored_;
  int sampling_exponent_;
};

// <tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;<sampling_exponent>;<rootPathRandom>
class DynatraceTracestate {
public:
  DynatraceTracestate(const std::string& tenant_id, const std::string& cluster_id) {
    key_ = absl::StrCat(absl::string_view(tenant_id), "-", absl::string_view(cluster_id), "@dt");
  }

  std::string getKey() const { return key_; };

  bool keyMatches(const std::string& key) { return (key_.compare(key) == 0); }

private:
  std::string key_;
};
} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
