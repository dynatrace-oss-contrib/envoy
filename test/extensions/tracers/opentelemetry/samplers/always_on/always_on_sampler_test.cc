#include <string>

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/always_on_sampler.pb.h"

#include "source/extensions/tracers/opentelemetry/samplers/always_on/always_on_sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"

#include "test/mocks/server/tracer_factory_context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(AlwaysOnSamplerTest, TestWithInvalidParentContext) {
  envoy::extensions::tracers::opentelemetry::samplers::v3::AlwaysOnSamplerConfig config;
  NiceMock<Server::Configuration::MockTracerFactoryContext> context;
  auto sampler = std::make_shared<AlwaysOnSampler>(config, context);
  EXPECT_STREQ(sampler->getDescription().c_str(), "AlwaysOnSampler");

  absl::StatusOr<SpanContext> span_context = absl::InvalidArgumentError("no parent span");
  auto sampling_result =
      sampler->shouldSample(span_context, "operation_name", "12345",
                            ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RECORD_AND_SAMPLE);
  EXPECT_EQ(sampling_result.attributes, nullptr);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), "");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

TEST(AlwaysOnSamplerTest, TestWithValidParentContext) {
  envoy::extensions::tracers::opentelemetry::samplers::v3::AlwaysOnSamplerConfig config;
  NiceMock<Server::Configuration::MockTracerFactoryContext> context;
  auto sampler = std::make_shared<AlwaysOnSampler>(config, context);
  EXPECT_STREQ(sampler->getDescription().c_str(), "AlwaysOnSampler");

  SpanContext ctx("0", "12345", "45678", false, "some_tracestate");
  absl::StatusOr<SpanContext> span_context = ctx;
  auto sampling_result =
      sampler->shouldSample(span_context, "operation_name", "12345",
                            ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RECORD_AND_SAMPLE);
  EXPECT_EQ(sampling_result.attributes, nullptr);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), "some_tracestate");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy