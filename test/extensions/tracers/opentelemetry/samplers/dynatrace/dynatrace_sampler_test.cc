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
<<<<<<< HEAD
  DynatraceSamplerTest() {
    TestUtility::loadFromYaml(yaml_string_, config_);
    NiceMock<Server::Configuration::MockTracerFactoryContext> context;

    SamplerConfigFetcherPtr cf = std::make_unique<MockSamplerConfigFetcher>();
    sampler_ = std::make_unique<DynatraceSampler>(config_, context, std::move(cf));
    EXPECT_STREQ(sampler_->getDescription().c_str(), "DynatraceSampler");
  }
=======
  DynatraceSamplerTest() { TestUtility::loadFromYaml(yaml_string, config_); }
>>>>>>> 2705f51fef (extend test)

protected:
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracerFactoryContext_;
  envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config_;
};

TEST_F(DynatraceSamplerTest, TestGetDescription) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracerFactoryContext_, std::move(scf));
  EXPECT_STREQ(sampler.getDescription().c_str(), "DynatraceSampler");
}

// Verify sampler being invoked with no parent span context
TEST_F(DynatraceSamplerTest, TestWithoutParentContext) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracerFactoryContext_, std::move(scf));

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

TEST_F(DynatraceSamplerTest, TestSampling) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();

  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");

  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));

  auto timer =
      new NiceMock<Event::MockTimer>(&tracerFactoryContext_.server_factory_context_.dispatcher_);
  ON_CALL(tracerFactoryContext_.server_factory_context_.dispatcher_, createTimer_(_))
      .WillByDefault(Invoke([timer](Event::TimerCb) { return timer; }));

  DynatraceSampler sampler(config_, tracerFactoryContext_, std::move(scf));
  Tracing::TestTraceContextImpl trace_context_1{};
  trace_context_1.context_method_ = "GET";
  trace_context_1.context_path_ = "/path";
  Tracing::TestTraceContextImpl trace_context_2{};
  trace_context_2.context_method_ = "POST";
  trace_context_2.context_path_ = "/path";
  Tracing::TestTraceContextImpl trace_context_3{};
  trace_context_3.context_method_ = "POST";
  trace_context_3.context_path_ = "/another_path";

  // send  requests
  for (int i = 0; i < 180; i++) {
    sampler.shouldSample({}, trace_id, "operation_name",
                         ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, trace_context_1,
                         {});
    sampler.shouldSample({}, trace_id, "operation_name",
                         ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, trace_context_2,
                         {});
  }

  sampler.shouldSample({}, trace_id, "operation_name",
                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, trace_context_3,
                       {});

  // sampler should read update sampling exponents
  timer->invokeCallback();

  // the sampler should not sample every span for 'trace_context_1'
  // we call it again 10 times. This should be enough to get at least one ignored span
  // 'i' is used as 'random trace_id'
  bool ignored = false;
  for (int i = 0; i < 10; i++) {
    auto result = sampler.shouldSample({}, std::to_string(i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_1, {});
    if (!result.isSampled()) {
      ignored = true;
      break;
    }
  }
  EXPECT_TRUE(ignored);

  // trace_context_3 should be sampled
  for (int i = 0; i < 10; i++) {
    auto result = sampler.shouldSample({}, std::to_string(i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_2, {});
    EXPECT_TRUE(result.isSampled());
  }
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
