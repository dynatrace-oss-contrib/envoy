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
      : request_(&tracerFactoryContext_.server_factory_context_.cluster_manager_
                      .thread_local_cluster_.async_client_) {
    const std::string yaml_string = R"EOF(
      cluster: "cluster_name"
      uri: "https://testhost.com/otlp/v1/traces"
      timeout: 0.250s
    )EOF";
    TestUtility::loadFromYaml(yaml_string, http_uri_);

    ON_CALL(tracerFactoryContext_.server_factory_context_.cluster_manager_,
            getThreadLocalCluster(_))
        .WillByDefault(Return(
            &tracerFactoryContext_.server_factory_context_.cluster_manager_.thread_local_cluster_));
    timer_ =
        new NiceMock<Event::MockTimer>(&tracerFactoryContext_.server_factory_context_.dispatcher_);
    ON_CALL(tracerFactoryContext_.server_factory_context_.dispatcher_, createTimer_(_))
        .WillByDefault(Invoke([this](Event::TimerCb) { return timer_; }));
  }

protected:
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracerFactoryContext_;
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
  EXPECT_CALL(tracerFactoryContext_.server_factory_context_.cluster_manager_.thread_local_cluster_
                  .async_client_,
              send_(MessageMatcher("unused-but-machtes-requires-an-arg"), _, _));
  SamplerConfigFetcher configFetcher(tracerFactoryContext_, http_uri_, "tokenval");
  timer_->invokeCallback();
}

// Test receiving a response with code 200 and valid json
TEST_F(SamplerConfigFetcherTest, TestResponseOk) {
  SamplerConfigFetcher configFetcher(tracerFactoryContext_, http_uri_, "tokenXASSD");
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  message->body().add("{\n \"rootSpansPerMinute\" : 4356 \n }");
  configFetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(configFetcher.getSamplerConfig().getRootSpansPerMinute(), 4356);
  EXPECT_TRUE(timer_->enabled());
}

// Test receiving a response with code 200 and unexpected json
TEST_F(SamplerConfigFetcherTest, TestResponseOkInvalidJson) {
  SamplerConfigFetcher configFetcher(tracerFactoryContext_, http_uri_, "tokenXASSD");
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  message->body().add("{\n ");
  configFetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(configFetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

// Test receiving a response with code != 200
TEST_F(SamplerConfigFetcherTest, TestResponseErrorCode) {
  SamplerConfigFetcher configFetcher(tracerFactoryContext_, http_uri_, "tokenXASSD");
  timer_->invokeCallback();

  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "401"}}}));
  message->body().add("{\n \"rootSpansPerMinute\" : 4356 \n }");
  configFetcher.onSuccess(request_, std::move(message));
  EXPECT_EQ(configFetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

// Test sending failed
TEST_F(SamplerConfigFetcherTest, TestOnFailure) {
  SamplerConfigFetcher configFetcher(tracerFactoryContext_, http_uri_, "tokenXASSD");
  timer_->invokeCallback();
  configFetcher.onFailure(request_, Http::AsyncClient::FailureReason::Reset);
  EXPECT_EQ(configFetcher.getSamplerConfig().getRootSpansPerMinute(),
            SamplerConfig::ROOT_SPANS_PER_MINUTE_DEFAULT);
  EXPECT_TRUE(timer_->enabled());
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
