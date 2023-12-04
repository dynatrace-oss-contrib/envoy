#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"

#include "source/common/common/enum_to_int.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

static constexpr std::chrono::seconds INITIAL_TIMER_DURATION{10};
static constexpr std::chrono::minutes TIMER_INTERVAL{5};

SamplerConfigFetcher::SamplerConfigFetcher(Server::Configuration::TracerFactoryContext& context,
                                           const envoy::config::core::v3::HttpService& http_service)
    : cluster_manager_(context.serverFactoryContext().clusterManager()),
      http_service_(http_service), sampler_config_() {

  for (const auto& header_value_option : http_service_.request_headers_to_add()) {
    parsed_headers_to_add_.push_back({Http::LowerCaseString(header_value_option.header().key()),
                                      header_value_option.header().value()});
  }

  timer_ = context.serverFactoryContext().mainThreadDispatcher().createTimer([this]() -> void {
    const auto thread_local_cluster =
        cluster_manager_.getThreadLocalCluster(http_service_.http_uri().cluster());
    if (thread_local_cluster == nullptr) {
      ENVOY_LOG(error, "SamplerConfigFetcher failed: [cluster = {}] is not configured",
                http_service_.http_uri().cluster());
    } else {
      Http::RequestMessagePtr message = Http::Utility::prepareHeaders(http_service_.http_uri());
      // TODO: set path
      // message->headers().setPath("path/to/sampler/service");
      message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
      // Add all custom headers to the request.
      for (const auto& header_pair : parsed_headers_to_add_) {
        message->headers().setReference(header_pair.first, header_pair.second);
      }
      active_request_ = thread_local_cluster->httpAsyncClient().send(
          std::move(message), *this,
          Http::AsyncClient::RequestOptions().setTimeout(std::chrono::milliseconds(6000)));
    }
  });

  timer_->enableTimer(std::chrono::seconds(INITIAL_TIMER_DURATION));
}

SamplerConfigFetcher::~SamplerConfigFetcher() {
  if (active_request_) {
    active_request_->cancel();
  }
}

void SamplerConfigFetcher::onSuccess(const Http::AsyncClient::Request& /*request*/,
                                     Http::ResponseMessagePtr&& http_response) {
  onRequestDone();
  const auto response_code = Http::Utility::getResponseStatus(http_response->headers());
  if (response_code != enumToInt(Http::Code::OK)) {
    ENVOY_LOG(error, "SamplerConfigFetcher received a non-success status code: {}", response_code);
  } else {
    ENVOY_LOG(info, "SamplerConfigFetcher received success status code: {}", response_code);
    sampler_config_.parse(http_response->bodyAsString());
  }
}

void SamplerConfigFetcher::onFailure(const Http::AsyncClient::Request& /*request*/,
                                     Http::AsyncClient::FailureReason reason) {
  onRequestDone();
  ENVOY_LOG(info, "The OTLP export request failed. Reason {}", enumToInt(reason));
}

void SamplerConfigFetcher::onRequestDone() {
  // TODO: should we re-enable timer after send() to avoid having the request duration added to the
  // timer? If so, we would need a list containing the active requests (not a single pointer)
  active_request_ = nullptr;
  timer_->enableTimer(std::chrono::seconds(TIMER_INTERVAL));
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
