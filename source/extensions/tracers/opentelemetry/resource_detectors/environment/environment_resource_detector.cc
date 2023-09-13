#include "environment_resource_detector.h"

#include <sstream>
#include <string>

#include "source/common/config/datasource.h"
#include "source/extensions/tracers/opentelemetry/resource_detectors/resource_detector.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

constexpr absl::string_view kOtelResourceAttributesEnv = "OTEL_RESOURCE_ATTRIBUTES";

/**
 * @brief Detects a resource from the OTEL_RESOURCE_ATTRIBUTES environment variable
 * Based on the OTel C++ SDK:
 * https://github.com/open-telemetry/opentelemetry-cpp/blob/v1.11.0/sdk/src/resource/resource_detector.cc
 *
 * @return Resource A resource with the attributes from the OTEL_RESOURCE_ATTRIBUTES environment
 * variable.
 */
Resource EnvironmentResourceDetector::detect() {
  envoy::config::core::v3::DataSource ds;
  ds.set_environment_variable(kOtelResourceAttributesEnv);

  Resource resource;
  resource.schemaUrl_ = "";

  auto attributes_str = Config::DataSource::read(ds, true, context_.serverFactoryContext().api());
  if (attributes_str.empty()) {
    return resource;
  }

  std::istringstream iss(attributes_str);
  std::string token;
  while (std::getline(iss, token, ',')) {
    size_t pos = token.find('=');
    std::string key = token.substr(0, pos);
    std::string value = token.substr(pos + 1);
    resource.attributes_[key] = value;
  }
  return resource;
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
