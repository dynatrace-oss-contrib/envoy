#pragma once

#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

// Based on protocol change
// https://bitbucket.lab.dynatrace.org/projects/ONE/repos/oneagent-protocols/pull-requests/2482/overview
// which defines a message containing a version & bitmask which get serialized on subpath metadata.
// The serialized byte buffer is used via extension No 8 for "FW4" DT / "fw4" tracestate tags.

class TraceCaptureReason {

  using Version = uint8_t;
  using PayloadBitMask = uint64_t;

  static const size_t MAX_PAYLOAD_SIZE = sizeof(Version) + sizeof(PayloadBitMask);

public:
  enum { InvalidVersion = 0u, Version1 = 1u };

  // Bit mask values
  enum Reason : PayloadBitMask {
    Undefined = 0ull,
    Atm = 1ull << 0,
    Fixed = 1ull << 1,
    Custom = 1ull << 2,
    Mainframe = 1ull << 3,
    Serverless = 1ull << 4,
    Rum = 1ull << 5,
  };

  static TraceCaptureReason createInvalid() { return {false, 0}; }

  static TraceCaptureReason create(PayloadBitMask tcr_bitmask) { return {true, tcr_bitmask}; }

  static TraceCaptureReason create(const absl::string_view& payload_hex) {
    if (payload_hex.size() < 4) {
      // At least 1 byte for version and 1 byte for bitmask (2 hex chars each)
      return createInvalid();
    }

    // validate the size of the payload
    size_t payload_bytes = payload_hex.size() / 2;
    if (payload_bytes <= sizeof(Version) || payload_bytes > MAX_PAYLOAD_SIZE) {
      return createInvalid();
    }

    // Parse version (first byte, 2 hex chars)
    uint32_t version = 0;
    if (!absl::SimpleHexAtoi(payload_hex.substr(0, 2), &version) ||
        version != TraceCaptureReason::Version1) {
      return createInvalid();
    }

    uint64_t bitmask = 0;
    if (!absl::SimpleHexAtoi(payload_hex.substr(2), &bitmask)) {
      return createInvalid();
    }

    constexpr PayloadBitMask kValidMask = Atm | Fixed | Custom | Mainframe | Serverless | Rum;
    if ((bitmask & ~kValidMask) != 0) {
      return createInvalid();
    }
    return create(bitmask);
  }

  // Returns true if parsing was successful.
  bool isValid() const { return valid_; };

  std::vector<absl::string_view> toSpanAttributeValue() const {
    std::vector<absl::string_view> result;
    if (tcr_bitmask_ & Atm) {
      result.push_back("atm");
    }
    if (tcr_bitmask_ & Fixed) {
      result.push_back("fixed");
    }
    if (tcr_bitmask_ & Custom) {
      result.push_back("custom");
    }
    if (tcr_bitmask_ & Rum) {
      result.push_back("rum");
    }
    if (result.empty()) {
      result.push_back("unknown");
    }
    return result;
  }

  std::string bitmaskHex() const {
    // shortcut if only the first 8 bits are used, which will be the common case
    if (tcr_bitmask_ < 0x100) {
      return absl::StrCat(absl::Hex(static_cast<uint8_t>(tcr_bitmask_), absl::kZeroPad2));
    }
    std::string hex;
    bool started = false;
    for (int shift = 56; shift >= 0; shift -= 8) {
      uint8_t b = static_cast<uint8_t>((tcr_bitmask_ >> shift) & 0xFF);
      if (b != 0 || started || shift == 0) {
        absl::StrAppend(&hex, absl::Hex(b, absl::kZeroPad2));
        started = true;
      }
    }
    return hex;
  }

private:
  TraceCaptureReason(bool valid, PayloadBitMask tcr_bitmask)
      : valid_(valid), tcr_bitmask_(tcr_bitmask) {}

  const bool valid_;
  const PayloadBitMask tcr_bitmask_;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
