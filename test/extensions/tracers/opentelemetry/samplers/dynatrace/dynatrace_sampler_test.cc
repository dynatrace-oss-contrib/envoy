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

namespace {

const char* trace_id = "67a9a23155e1741b5b35368e08e6ece5";

const char* parent_span_id = "9d83def9a4939b7b";

const char* dt_tracestate_ignored =
    "9712ad40-980df25c@dt=fw4;4;4af38366;0;0;1;2;123;8eae;2h01;3h4af38366;4h00;5h01;"
    "6h67a9a23155e1741b5b35368e08e6ece5;7h9d83def9a4939b7b";
const char* dt_tracestate_sampled =
    "9712ad40-980df25c@dt=fw4;4;4af38366;0;0;0;0;123;8eae;2h01;3h4af38366;4h00;5h01;"
    "6h67a9a23155e1741b5b35368e08e6ece5;7h9d83def9a4939b7b";
const char* dt_tracestate_ignored_different_tenant =
    "6666ad40-980df25c@dt=fw4;4;4af38366;0;0;1;2;123;8eae;2h01;3h4af38366;4h00;5h01;"
    "6h67a9a23155e1741b5b35368e08e6ece5;7h9d83def9a4939b7b";

} // namespace

class MockSamplerConfigFetcher : public SamplerConfigFetcher {
public:
  MOCK_METHOD(const SamplerConfig&, getSamplerConfig, (), (const override));
};

class DynatraceSamplerTest : public testing::Test {

  const std::string yaml_string_ = R"EOF(
          tenant_id: "9712ad40"
          cluster_id: "980df25c"
  )EOF";

public:
  DynatraceSamplerTest() {
    TestUtility::loadFromYaml(yaml_string_, config_);
    NiceMock<Server::Configuration::MockTracerFactoryContext> context;

    SamplerConfigFetcherPtr cf = std::make_unique<MockSamplerConfigFetcher>();
    sampler_ = std::make_unique<DynatraceSampler>(config_, context, std::move(cf));
    EXPECT_STREQ(sampler_->getDescription().c_str(), "DynatraceSampler");
  }

protected:
  envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config_;
  std::unique_ptr<DynatraceSampler> sampler_;
};

// Verify sampler being invoked with no parent span context
TEST_F(DynatraceSamplerTest, TestWithoutParentContext) {
  auto sampling_result =
      sampler_->shouldSample(absl::nullopt, trace_id, "operation_name",
                             ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), "9712ad40-980df25c@dt=fw4;0;0;0;0;0;0;95");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

// Verify sampler being invoked with existing Dynatrace trace state tag set
TEST_F(DynatraceSamplerTest, TestWithParentContext) {
  SpanContext parent_context =
      SpanContext("00", "0af7651916cd43dd8448eb211c80319c", "b7ad6b7169203331", true,
                  "ot=foo:bar,9712ad40-980df25c@dt=fw4;0;0;0;0;0;1;0");

  SamplingResult sampling_result =
      sampler_->shouldSample(parent_context, "operation_name", "parent_span",
                             ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});

  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(),
               "ot=foo:bar,9712ad40-980df25c@dt=fw4;0;0;0;0;0;1;0");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
