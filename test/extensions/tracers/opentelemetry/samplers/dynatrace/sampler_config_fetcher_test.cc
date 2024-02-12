#include <memory>
#include <string>
#include <utility>

#include "envoy/config/core/v3/http_uri.pb.h"

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/tracer_factory_context.h"
#include "test/mocks/tracing/mocks.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

class SamplerConfigFetcherTest : public testing::Test {
public:
  SamplerConfigFetcherTest()
      : request_(&tracer_factory_context_.server_factory_context_.cluster_manager_
                      .thread_local_cluster_.async_client_) {
    const std::string yaml_string = R"EOF(
      cluster: "cluster_name"
      uri: "https://testhost.com/otlp/v1/traces"
      timeout: 0.250s
    )EOF";
    TestUtility::loadFromYaml(yaml_string, http_uri_);

    ON_CALL(tracer_factory_context_.server_factory_context_.cluster_manager_,
            getThreadLocalCluster(_))
        .WillByDefault(Return(&tracer_factory_context_.server_factory_context_.cluster_manager_
                                   .thread_local_cluster_));
    timer_ = new NiceMock<Event::MockTimer>(
        &tracer_factory_context_.server_factory_context_.dispatcher_);
    ON_CALL(tracer_factory_context_.server_factory_context_.dispatcher_, createTimer_(_))
        .WillByDefault(Invoke([this](Event::TimerCb) { return timer_; }));
  }

protected:
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracer_factory_context_;
  envoy::config::core::v3::HttpUri http_uri_;
  NiceMock<Event::MockTimer>* timer_;
  Http::MockAsyncClientRequest request_;
};

MATCHER_P(MessageMatcher, unusedArg, "") {
  // prefix 'Api-Token' should be added to 'tokenval' set via SamplerConfigFetcher constructor
  const Http::LowerCaseString auth{"authorization"};
  return (arg->headers().get(auth)[0]->value().getStringView() == "Api-Token tokenval") &&
         (arg->headers().get(Http::Headers::get().Path)[0]->value().getStringView() ==
          "/otlp/v1/traces") &&
         (arg->headers().get(Http::Headers::get().Host)[0]->value().getStringView() ==
          "testhost.com") &&
         (arg->headers().get(Http::Headers::get().Method)[0]->value().getStringView() == "GET");
}

// Test a request is sent if timer fires
TEST_F(SamplerConfigFetcherTest, TestRequestIsSent) {
  EXPECT_CALL(tracer_factory_context_.server_factory_context_.cluster_manager_.thread_local_cluster_
                  .async_client_,
              send_(MessageMatcher("unused-but-machtes-requires-an-arg"), _, _));
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenval",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();
}

// Test receiving a response with code 200 and valid json
TEST_F(SamplerConfigFetcherTest, TestResponseOk) {
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  message->body().add("{\n \"rootSpansPerMinute\" : 4356 \n }");
  config_fetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(config_fetcher.getSamplerConfig().getRootSpansPerMinute(), 4356);
  EXPECT_TRUE(timer_->enabled());
}

// Test receiving a response with code 200 and unexpected json
TEST_F(SamplerConfigFetcherTest, TestResponseOkInvalidJson) {
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  message->body().add("{\n ");
  config_fetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(config_fetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

// Test receiving a response with code != 200
TEST_F(SamplerConfigFetcherTest, TestResponseErrorCode) {
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "401"}}}));
  message->body().add("{\n \"rootSpansPerMinute\" : 4356 \n }");
  config_fetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(config_fetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

// Test sending failed
TEST_F(SamplerConfigFetcherTest, TestOnFailure) {
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();
  config_fetcher.onFailure(request_, Http::AsyncClient::FailureReason::Reset);
  EXPECT_EQ(config_fetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

// Test calling onBeforeFinalizeUpstreamSpan
TEST_F(SamplerConfigFetcherTest, TestOnBeforeFinalizeUpstreamSpan) {
  Tracing::MockSpan child_span_;
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  // onBeforeFinalizeUpstreamSpan() is an empty method, nothing should happen
  config_fetcher.onBeforeFinalizeUpstreamSpan(child_span_, nullptr);
}

// Test invoking the timer if no cluster can be found
TEST_F(SamplerConfigFetcherTest, TestNoCluster) {
  // simulate no configured cluster, return nullptr.
  ON_CALL(tracer_factory_context_.server_factory_context_.cluster_manager_,
          getThreadLocalCluster(_))
      .WillByDefault(Return(nullptr));
  SamplerConfigFetcherImpl config_fetcher(tracer_factory_context_, http_uri_, "tokenXASSD",
                                          SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  timer_->invokeCallback();
  // should not crash or throw.
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
