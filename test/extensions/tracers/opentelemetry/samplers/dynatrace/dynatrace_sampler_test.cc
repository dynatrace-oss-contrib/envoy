#include <memory>
#include <string>

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

#include "test/mocks/server/tracer_factory_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class DynatraceSamplerTest : public testing::Test {

  const std::string yaml_string = R"EOF(
          tenant_id: "9712ad40"
          cluster_id: "980df25c"
  )EOF";

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

public:
  DynatraceSamplerTest() {
    TestUtility::loadFromYaml(yaml_string, config_);
    TestUtility::loadFromYaml(otel_yaml, opentelemetry_config_);
    NiceMock<Server::Configuration::MockTracerFactoryContext> context;
    sampler_ = std::make_unique<DynatraceSampler>(config_, context, opentelemetry_config_);
    EXPECT_STREQ(sampler_->getDescription().c_str(), "DynatraceSampler");
  }

protected:
  envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config_;
  envoy::config::trace::v3::OpenTelemetryConfig opentelemetry_config_;
  std::unique_ptr<DynatraceSampler> sampler_;
};

// Verify sampler being invoked with an invalid span context
TEST_F(DynatraceSamplerTest, TestWithoutParentContext) {

  auto sampling_result =
      sampler_->shouldSample(absl::nullopt, "operation_name", "12345",
                             ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RECORD_AND_SAMPLE);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), "9712ad40-980df25c@dt=fw4;0;0;0;0;0;1;0");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
