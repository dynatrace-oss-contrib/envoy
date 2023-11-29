#include "envoy/registry/registry.h"

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/config.h"

#include "test/mocks/server/tracer_factory_context.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(DynatraceSamplerFactoryTest, Test) {
  auto* factory = Registry::FactoryRegistry<SamplerFactory>::getFactory(
      "envoy.tracers.opentelemetry.samplers.dynatrace");
  ASSERT_NE(factory, nullptr);
  EXPECT_STREQ(factory->name().c_str(), "envoy.tracers.opentelemetry.samplers.dynatrace");
  EXPECT_NE(factory->createEmptyConfigProto(), nullptr);

  envoy::config::core::v3::TypedExtensionConfig typed_config;
  const std::string sampler_yaml = R"EOF(
    name: envoy.tracers.opentelemetry.samplers.dynatrace
    typed_config:
        "@type": type.googleapis.com/envoy.extensions.tracers.opentelemetry.samplers.v3.DynatraceSamplerConfig
  )EOF";
  TestUtility::loadFromYaml(sampler_yaml, typed_config);

  const std::string otel_yaml = R"EOF(
    http_service:
      http_uri:
        cluster: "my_o11y_backend"
        uri: "https://some-o11y.com/otlp/v1/traces"
        timeout: 0.250s
      request_headers_to_add:
      - header:
          key: "Authorization"
          value: "auth-token"
    )EOF";
  envoy::config::trace::v3::OpenTelemetryConfig opentelemetry_config;
  TestUtility::loadFromYaml(otel_yaml, opentelemetry_config);

  NiceMock<Server::Configuration::MockTracerFactoryContext> context;
  EXPECT_NE(factory->createSampler(typed_config.typed_config(), context, opentelemetry_config),
            nullptr);
  EXPECT_STREQ(factory->name().c_str(), "envoy.tracers.opentelemetry.samplers.dynatrace");
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
