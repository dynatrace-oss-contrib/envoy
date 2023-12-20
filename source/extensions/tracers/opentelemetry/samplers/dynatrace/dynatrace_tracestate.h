#pragma once

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class FW4Tag {
public:
  static FW4Tag createInvalid();

  static FW4Tag create(bool ignored, int sampling_exponent, int root_path_random_);

  static FW4Tag create(const std::string& value);

  std::string asString() const;

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  int getSamplingExponent() const { return sampling_exponent_; };

private:
  FW4Tag(bool valid, bool ignored, int sampling_exponent, int root_path_random_)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent),
        root_path_random_(root_path_random_) {}

  bool valid_;
  bool ignored_;
  int sampling_exponent_;
  int root_path_random_;
};

// <tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;<sampling_exponent>;<rootPathRandom>
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
