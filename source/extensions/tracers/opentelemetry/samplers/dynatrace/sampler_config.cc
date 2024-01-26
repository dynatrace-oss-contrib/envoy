#include <utility>

#include "source/common/json/json_loader.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

void SamplerConfig::parse(const std::string& json) {
  const auto result = Envoy::Json::Factory::loadFromStringNoThrow(json);
  if (result.ok()) {
    const auto obj = result.value();
    if (obj->hasObject("rootSpansPerMinute")) {
      const auto value = obj->getInteger("rootSpansPerMinute", ROOT_SPANS_PER_MINUTE_DEFAULT);
      root_spans_per_minute_.store(value);
      return;
    }
    (void)obj;
  }
  // didn't get a value, reset to default
  root_spans_per_minute_.store(ROOT_SPANS_PER_MINUTE_DEFAULT);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
