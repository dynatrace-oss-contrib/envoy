#pragma once

#include <array>
#include <memory>
#include <string>

#include "envoy/common/pure.h"

#include "absl/strings/match.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

/**
 * @brief A file reader that reads the content of the Dynatrace metadata enrichment files.
 * The Dynatrace enrichment file is a "virtual" file, meaning it does not physically exists on disk.
 * When the Dynatrace OneAgent is deployed on the host, file read calls are intercepted by the
 * OneAgent, which then mimics the existence of the enrichment files. This allow obtaining not only
 * host, but also process related information.
 *
 * Because of the virtual file, using the file reader in Envoy will not work, since it validates the
 * existence of the file.
 *
 * @see
 * https://docs.dynatrace.com/docs/shortlink/enrichment-files#oneagent-virtual-files
 *
 */
class DynatraceMetadataFileReader {
public:
  virtual ~DynatraceMetadataFileReader() = default;

  /**
   * @brief Reads the enrichment file and returns the enrichment metadata.
   *
   * @param file_name The file name.
   * @return const std::string String (java-like properties) containing the enrichment metadata.
   */
  virtual const std::string readEnrichmentFile(const std::string& file_name) PURE;
};

using DynatraceMetadataFileReaderPtr = std::unique_ptr<DynatraceMetadataFileReader>;

class DynatraceMetadataFileReaderImpl : public DynatraceMetadataFileReader {
public:
  const std::string readEnrichmentFile(const std::string& file_name) override;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
