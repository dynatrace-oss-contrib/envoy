#include "source/extensions/tracers/opentelemetry/resource_detectors/dynatrace/dynatrace_resource_detector.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "source/common/config/datasource.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {
namespace {

void addAttributes(std::string content, Resource& resource) {
  for (const auto& line : StringUtil::splitToken(content, "\n")) {
    const auto keyValue = StringUtil::splitToken(line, "=");
    if (keyValue.size() != 2) {
      continue;
    }

    const std::string key = std::string(keyValue[0]);
    const std::string value = std::string(keyValue[1]);
    resource.attributes_[key] = value;
  }
}

} // namespace

Resource DynatraceResourceDetector::detect() {
  envoy::config::core::v3::DataSource ds;

  Resource resource;
  resource.schemaUrl_ = "";
  int failureCount = 0;

  for (const auto& file_name : DT_METADATA_FILES) {
    ds.clear_filename();
    ds.set_filename(file_name);

    TRY_NEEDS_AUDIT {
      std::string content = dynatrace_file_reader_->readEnrichmentFile(file_name);
      if (content.empty()) {
        failureCount++;
      } else {
        addAttributes(content, resource);
      }
    }
    END_TRY catch (const EnvoyException& e) { failureCount++; }
  }

  // Tried all enrichment files and none were either found or worked
  // This means either the Dynatrace OneAgent or the Operator are not installed/deployed.
  if (failureCount == DT_METADATA_FILES.size()) {
    throw EnvoyException(
        "Dynatrace OpenTelemetry resource detector is configured but failed to find the enrichment "
        "files. Make sure Dynatrace OneAgent or the k8s operator is installed");
  }

  return resource;
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
