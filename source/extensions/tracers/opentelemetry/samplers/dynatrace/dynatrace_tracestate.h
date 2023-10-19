#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "murmur.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class FW4Tag {

  static FW4Tag create(std::string& value) {
    std::vector<absl::string_view> tracestate_components =
        absl::StrSplit(value, ';', absl::AllowEmpty());
    if (tracestate_components.size() < 7) {
      return {false, false, 0};
    }

    if (tracestate_components[0] != "fw4") {
      return {false, false, 0};
    }
    bool ignored = tracestate_components[5] == "1";
    int sampling_exponent = std::stoi(std::string(tracestate_components[6]));
    return {true, ignored, sampling_exponent};
  }

  FW4Tag(bool valid, bool ignored, int sampling_exponent)
      : valid_(valid), ignored_(ignored), sampling_exponent_(sampling_exponent) {}

  bool isValid() const { return valid_; };
  bool isIgnored() const { return ignored_; };
  int getSamplingExponent() const { return sampling_exponent_; };

  std::string createValue() const {
    std::string ret = absl::StrCat("dt=fw4;0;0;0;0;", ignored_ ? "1" : "0", sampling_exponent_);
    return ret;
  }

private:
  bool valid_;
  bool ignored_;
  int sampling_exponent_;
};

// <tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;<sampling_exponent>;<rootPathRandom>
class DynatraceTracestate {
public:
  DynatraceTracestate() = default;

  DynatraceTracestate(const std::string& tenant_id, const std::string& cluster_id)
      : tenant_id_(tenant_id), cluster_id_(cluster_id) {
    key_ = absl::StrCat(absl::string_view(tenant_id_), "-", absl::string_view(cluster_id), "@dt");
  }

  std::string getKey() const { return key_; };

  bool isIgnored(const std::string& value) {
    std::vector<absl::string_view> tracestate_components =
        absl::StrSplit(value, ';', absl::AllowEmpty());
    if (tracestate_components.size() > 7) {
      return tracestate_components[5] == "1";
    }
    return false;
  }

  static std::string hash_extension(const std::string& extension) {
    uint64_t hash = murmurHash264A(extension.c_str(), extension.size());
    // hash &= 0xffff;
    uint16_t hash_16 = static_cast<uint16_t>(hash);
    std::stringstream stream;
    stream << std::hex << hash_16;
    std::string hash_as_hex(stream.str());
    return hash_as_hex;
  }

  // <tenantID>-<clusterID>@dt=fw4;0;0;0;0;<isIgnored>;<sampling_exponent>;<rootPathRandom>
  static DynatraceTracestate parse(const std::string& tracestate) {
    DynatraceTracestate ret;
    std::vector<absl::string_view> tracestate_components =
        absl::StrSplit(tracestate, ';', absl::AllowEmpty());
    if (tracestate_components.size() > 7) {
      std::vector<absl::string_view> tenant_cluster_et_al =
          absl::StrSplit(tracestate_components[0], '@', absl::SkipEmpty());
      if (tenant_cluster_et_al.size() != 2) {
        // TODO: error handling
        return {};
      }
      std::vector<absl::string_view> tenant_cluster =
          absl::StrSplit(tenant_cluster_et_al[0], '-', absl::SkipEmpty());
      if (tenant_cluster.size() != 2) {
        // TODO: error handling
        return {};
      }
      ret.tenant_id = tenant_cluster[0];
      ret.cluster_id = tenant_cluster[1];

      ret.is_ignored = tracestate_components[5];
      ret.sampling_exponent = tracestate_components[6];
      ret.path_info = tracestate_components[7];
      for (auto const& component : tracestate_components) {
        if (absl::StartsWith(component, "7h")) {
          ret.span_id = component;
        }
      }
    }
    return ret;
  }

  std::string tenant_id = "<tenant_id_t>";
  std::string cluster_id = "<cluster_id_t>";
  static constexpr absl::string_view at_dt_format = "@dt=fw4";
  static constexpr absl::string_view server_id = "0";
  static constexpr absl::string_view agent_d = "0";
  static constexpr absl::string_view tag_id = "0";
  static constexpr absl::string_view link_id = "0";
  std::string is_ignored = "0";
  std::string sampling_exponent = "0";
  std::string path_info = "0";
  std::string span_id = "";

  std::string toString() {
    std::string tracestate = absl::StrCat(
        absl::string_view(tenant_id), "-", absl::string_view(cluster_id), at_dt_format, ";",
        server_id, ";", agent_d, ";", tag_id, ";", link_id, ";", absl::string_view(is_ignored), ";",
        absl::string_view(sampling_exponent), ";", absl::string_view(path_info));

    //    Which is: "abcdabcd-77@dt=fw4;8;66666666;111;99;0;0;66;;8cef;2h01;7h293e72b548735604"
    if (!span_id.empty()) {
      std::string tmp(tracestate);
      // https://oaad.lab.dynatrace.org/agent/concepts/purepath/tagging/#forward-tags-fw
      std::string extension = ";7h" + span_id;
      std::string hash_as_hex = hash_extension(extension);
      tracestate = absl::StrCat(absl::string_view(tmp), ";", absl::string_view(hash_as_hex),
                                absl::string_view(extension));
    }
    return tracestate;
  }

private:
  std::string tenant_id_;
  std::string cluster_id_;
  std::string key_;
};
} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
