#include <memory>
#include <string>
#include <utility>

#include "envoy/config/core/v3/http_service.pb.h"

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

protected:
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracerFactoryContext_;
  envoy::config::core::v3::HttpService http_service_;
  NiceMock<Event::MockTimer>* timer_;
};

TEST_F(SamplerConfigFetcherTest, Test) {
  NiceMock<Envoy::Server::Configuration::MockTracerFactoryContext> tracerFactoryContext_;

  const std::string yaml_string = R"EOF(
      http_uri:
        cluster: "cluster_name"
        uri: "https://some-o11y.com/otlp/v1/traces"
        timeout: 0.250s
      request_headers_to_add:
      - header:
          key: "Authorization"
          value: "auth-token"
    )EOF";

  TestUtility::loadFromYaml(yaml_string, http_service_);

  ON_CALL(tracerFactoryContext_.server_factory_context_.cluster_manager_, getThreadLocalCluster(_))
      .WillByDefault(Return(
          &tracerFactoryContext_.server_factory_context_.cluster_manager_.thread_local_cluster_));

  timer_ =
      new NiceMock<Event::MockTimer>(&tracerFactoryContext_.server_factory_context_.dispatcher_);
  ON_CALL(tracerFactoryContext_.server_factory_context_.dispatcher_, createTimer_(_))
      .WillByDefault(Invoke([this](Event::TimerCb) { return timer_; }));

  EXPECT_CALL(tracerFactoryContext_.server_factory_context_.cluster_manager_.thread_local_cluster_
                  .async_client_,
              send_(_, _, _));

  SamplerConfigFetcher f(tracerFactoryContext_, http_service_);
  timer_->invokeCallback();

  Http::MockAsyncClientRequest request_(&tracerFactoryContext_.server_factory_context_
                                             .cluster_manager_.thread_local_cluster_.async_client_);
  Http::ResponseMessagePtr message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  message->body().add("{\n \"rootSpansPerMinute\" : 4356 \n }");
  f.onSuccess(request_, std::move(message));
  EXPECT_EQ(f.getSamplerConfig().getRootSpansPerMinute(), 4356);
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
