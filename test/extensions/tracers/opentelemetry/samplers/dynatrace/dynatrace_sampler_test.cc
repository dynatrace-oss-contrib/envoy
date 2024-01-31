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
  DynatraceSamplerTest() { TestUtility::loadFromYaml(yaml_string_, config_); }

protected:
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracer_factory_context_;
  envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig config_;
};

TEST_F(DynatraceSamplerTest, TestGetDescription) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));
  EXPECT_STREQ(sampler.getDescription().c_str(), "DynatraceSampler");
}

// Verify sampler being invoked with an invalid/empty span context
TEST_F(DynatraceSamplerTest, TestWithoutParentContext) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();

  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");
  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));
  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));

  auto sampling_result =
      sampler.shouldSample(absl::nullopt, trace_id, "operation_name",
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

  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));
  SamplingResult sampling_result =
      sampler.shouldSample(parent_context, "operation_name", "parent_span",
                           ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});

  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(),
               "ot=foo:bar,9712ad40-980df25c@dt=fw4;0;0;0;0;0;1;0");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

// Verify sampler being invoked with parent span context
TEST_F(DynatraceSamplerTest, TestWithUnknownParentContext) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");
  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));

  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));

  SpanContext parent_context("00", trace_id, parent_span_id, true, "some_vendor=some_value");

  auto sampling_result =
      sampler.shouldSample(parent_context, trace_id, "operation_name",
                           ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(),
               "9712ad40-980df25c@dt=fw4;0;0;0;0;0;0;95,some_vendor=some_value");
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

// Verify sampler being invoked with dynatrace trace parent
TEST_F(DynatraceSamplerTest, TestWithDynatraceParentContextSampled) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));

  SpanContext parent_context("00", trace_id, parent_span_id, true, dt_tracestate_sampled);

  auto sampling_result =
      sampler.shouldSample(parent_context, trace_id, "operation_name",
                           ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), dt_tracestate_sampled);
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

// Verify sampler being invoked with dynatrace trace parent where ignored flag is set
TEST_F(DynatraceSamplerTest, TestWithDynatraceParentContextIgnored) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));
  SpanContext parent_context("00", trace_id, parent_span_id, true, dt_tracestate_ignored);

  auto sampling_result =
      sampler.shouldSample(parent_context, trace_id, "operation_name",
                           ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  EXPECT_EQ(sampling_result.decision, Decision::Drop);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  EXPECT_STREQ(sampling_result.tracestate.c_str(), dt_tracestate_ignored);
  EXPECT_FALSE(sampling_result.isRecording());
  EXPECT_FALSE(sampling_result.isSampled());
}

// Verify sampler being invoked with dynatrace trace parent from a different tenant
TEST_F(DynatraceSamplerTest, TestWithDynatraceParentContextFromDifferentTenant) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");

  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));

  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));
  SpanContext parent_context("00", trace_id, parent_span_id, true,
                             dt_tracestate_ignored_different_tenant);

  auto sampling_result =
      sampler.shouldSample(parent_context, trace_id, "operation_name",
                           ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER, {}, {});
  // sampling decision on tracestate should be ignored because it is from a different tenant.
  EXPECT_EQ(sampling_result.decision, Decision::RecordAndSample);
  EXPECT_EQ(sampling_result.attributes->size(), 1);
  const char* exptected =
      "9712ad40-980df25c@dt=fw4;0;0;0;0;0;0;95,6666ad40-980df25c@dt=fw4;4;4af38366;0;0;1;2;123;"
      "8eae;2h01;3h4af38366;4h00;5h01;6h67a9a23155e1741b5b35368e08e6ece5;7h9d83def9a4939b7b";
  EXPECT_STREQ(sampling_result.tracestate.c_str(), exptected);
  EXPECT_TRUE(sampling_result.isRecording());
  EXPECT_TRUE(sampling_result.isSampled());
}

// Verify sampler being called during warmup phase (no recent top_k available)
TEST_F(DynatraceSamplerTest, TestWarmup) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();
  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");
  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));

  Tracing::TestTraceContextImpl trace_context_1{};
  trace_context_1.context_method_ = "GET";
  trace_context_1.context_path_ = "/path";

  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));

  // timer is not invoked, because we want to test warm up phase.
  // we use 200 as threshold. As long as number of requests is < (threshold/2), exponent should be 0
  uint32_t ignored = 0;
  uint32_t sampled = 0;
  for (int i = 0; i < 99; i++) {
    auto result = sampler.shouldSample({}, std::to_string(1000 + i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_1, {});
    result.isSampled() ? sampled++ : ignored++;
  }
  EXPECT_EQ(ignored, 0);
  EXPECT_EQ(sampled, 99);

  // next (threshold/2) spans will get exponent 1, every second span will be sampled
  for (int i = 0; i < 100; i++) {
    auto result = sampler.shouldSample({}, std::to_string(1000 + i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_1, {});
    result.isSampled() ? sampled++ : ignored++;
  }
  // should be 50, but the used "random" in shouldSample does not produce the same odd/even numbers.
  EXPECT_EQ(ignored, 41);
  EXPECT_EQ(sampled, 158);

  for (int i = 0; i < 100; i++) {
    auto result = sampler.shouldSample({}, std::to_string(1000 + i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_1, {});
    result.isSampled() ? sampled++ : ignored++;
  }
  EXPECT_EQ(ignored, 113);
  EXPECT_EQ(sampled, 186);

  for (int i = 0; i < 700; i++) {
    auto result = sampler.shouldSample({}, std::to_string(1000 + i), "operation_name",
                                       ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER,
                                       trace_context_1, {});
    result.isSampled() ? sampled++ : ignored++;
  }
  EXPECT_EQ(ignored, 791);
  EXPECT_EQ(sampled, 208);
}

TEST_F(DynatraceSamplerTest, TestSampling) {
  auto scf = std::make_unique<MockSamplerConfigFetcher>();

  SamplerConfig config;
  // config should allow 200 root spans per minute
  config.parse("{\n \"rootSpansPerMinute\" : 200 \n }");

  EXPECT_CALL(*scf, getSamplerConfig()).WillRepeatedly(testing::ReturnRef(config));

  auto timer =
      new NiceMock<Event::MockTimer>(&tracer_factory_context_.server_factory_context_.dispatcher_);
  ON_CALL(tracer_factory_context_.server_factory_context_.dispatcher_, createTimer_(_))
      .WillByDefault(Invoke([timer](Event::TimerCb) { return timer; }));

  DynatraceSampler sampler(config_, tracer_factory_context_, std::move(scf));
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
