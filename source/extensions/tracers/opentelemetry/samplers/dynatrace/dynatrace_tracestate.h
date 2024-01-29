#pragma once

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

// dynatrace trace state entry will be:
// <tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;<sampling_exponent>;<pathInfo>

class FW4Tag {
public:
  static FW4Tag createInvalid();

  static FW4Tag create(bool ignored, uint32_t sampling_exponent, uint32_t root_path_random);

  static FW4Tag create(const std::string& value);

  std::string asString() const;

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  uint32_t getSamplingExponent() const { return sampling_exponent_; };
  uint32_t getPathInfo() const { return path_info_; };

private:
  FW4Tag(bool valid, bool ignored, uint32_t sampling_exponent, uint32_t path_info)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent),
        path_info_(path_info) {}

  bool valid_;
  bool ignored_;
  uint32_t sampling_exponent_;
  uint32_t path_info_;
};

class DtTracestateEntry {
public:
  DtTracestateEntry(const std::string& tenant_id, const std::string& cluster_id) {
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
