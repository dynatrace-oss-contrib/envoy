#pragma once

#include "envoy/extensions/tracers/opentelemetry/resource_detectors/v3/dynatrace_resource_detector.pb.h"

#include "source/common/common/logger.h"
#include "source/extensions/tracers/opentelemetry/resource_detectors/dynatrace/dynatrace_metadata_file_reader.h"
#include "source/extensions/tracers/opentelemetry/resource_detectors/resource_detector.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

const std::array<std::string, 3> DT_METADATA_FILES = {
    "dt_metadata_e617c525669e072eebe3d0f08212e8f2.properties",
    "/var/lib/dynatrace/enrichment/dt_metadata.properties",
    "/var/lib/dynatrace/enrichment/dt_host_metadata.properties"};

/**
 * @brief A resource detector that reads the content of the Dynatrace metadata enrichment files.
 * The Dynatrace enrichment file is a "virtual" file, meaning it does not physically exists on disk.
 * When the Dynatrace OneAgent is deployed on the host, file read calls are intercepted by the
 * OneAgent, which then mimics the existence of the enrichment files. This allow obtaining not only
 * host, but also process related information.
 *
 * In k8s, the OneAgent is often not present. In this case, the Dynatrace Operator
 * deploys a file containing the k8s-related enrichment host attributes instead.
 *
 * Since the resource detector is not capable of being aware of which environment Envoy is running
 * (host monitored by the OneAgent, inside a k8s cluster) the detector attempts to reads all
 * Dynatrace enrichment files. In such cases, reading some of these files will fail (e.g. in k8s,
 * when the OneAgent is not present) but this is expected and does not classify as a problem with
 * the detector.
 *
 * The resource detector only fails and stops Envoy when no enrichment files were found.
 *
 * @see
 * https://docs.dynatrace.com/docs/shortlink/enrichment-files
 *
 */
class DynatraceResourceDetector : public ResourceDetector, Logger::Loggable<Logger::Id::tracing> {
public:
  DynatraceResourceDetector(const envoy::extensions::tracers::opentelemetry::resource_detectors::
                                v3::DynatraceResourceDetectorConfig& config,
                            DynatraceMetadataFileReaderPtr dynatrace_file_reader)
      : config_(config), dynatrace_file_reader_(std::move(dynatrace_file_reader)) {}
  Resource detect() override;

private:
  const envoy::extensions::tracers::opentelemetry::resource_detectors::v3::
      DynatraceResourceDetectorConfig config_;
  DynatraceMetadataFileReaderPtr dynatrace_file_reader_;
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
