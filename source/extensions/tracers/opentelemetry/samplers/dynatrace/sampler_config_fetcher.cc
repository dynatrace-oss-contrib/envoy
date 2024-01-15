#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_fetcher.h"

#include "source/common/common/enum_to_int.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

static constexpr std::chrono::seconds INITIAL_TIMER_DURATION{10};
static constexpr std::chrono::minutes TIMER_INTERVAL{5};

SamplerConfigFetcherImpl::SamplerConfigFetcherImpl(
    Server::Configuration::TracerFactoryContext& context,
    const envoy::config::core::v3::HttpUri& http_uri, const std::string& token)
    : cluster_manager_(context.serverFactoryContext().clusterManager()), http_uri_(http_uri),
      parsed_authorization_header_to_add_(
          {Http::LowerCaseString("authorization"), absl::StrCat("Api-Token ", token)}),
      sampler_config_() {

  timer_ = context.serverFactoryContext().mainThreadDispatcher().createTimer([this]() -> void {
    const auto thread_local_cluster = cluster_manager_.getThreadLocalCluster(http_uri_.cluster());
    if (thread_local_cluster == nullptr) {
      ENVOY_LOG(error, "SamplerConfigFetcherImpl failed: [cluster = {}] is not configured",
                http_uri_.cluster());
    } else {
      Http::RequestMessagePtr message = Http::Utility::prepareHeaders(http_uri_);
      // TODO: set path once it is finalized in TI-8742
      // message->headers().setPath("path/to/sampler/service");
      message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
      message->headers().setReference(parsed_authorization_header_to_add_.first,
                                      parsed_authorization_header_to_add_.second);
      active_request_ = thread_local_cluster->httpAsyncClient().send(
          std::move(message), *this,
          Http::AsyncClient::RequestOptions().setTimeout(std::chrono::milliseconds(6000)));
    }
  });

  timer_->enableTimer(std::chrono::seconds(INITIAL_TIMER_DURATION));
}

SamplerConfigFetcherImpl::~SamplerConfigFetcherImpl() {
  if (active_request_) {
    active_request_->cancel();
  }
}

void SamplerConfigFetcherImpl::onSuccess(const Http::AsyncClient::Request& /*request*/,
                                         Http::ResponseMessagePtr&& http_response) {
  onRequestDone();
  const auto response_code = Http::Utility::getResponseStatus(http_response->headers());
  if (response_code == enumToInt(Http::Code::OK)) {
    // TODO: remove log
    ENVOY_LOG(info, "SamplerConfigFetcherImpl received success status code: {}", response_code);
    sampler_config_.parse(http_response->bodyAsString());
  } else {
    ENVOY_LOG(error, "SamplerConfigFetcherImpl received a non-success status code: {}",
              response_code);
  }
}

void SamplerConfigFetcherImpl::onFailure(const Http::AsyncClient::Request& /*request*/,
                                         Http::AsyncClient::FailureReason reason) {
  onRequestDone();
  ENVOY_LOG(info, "The OTLP export request failed. Reason {}", enumToInt(reason));
}

void SamplerConfigFetcherImpl::onRequestDone() {
  active_request_ = nullptr;
  timer_->enableTimer(std::chrono::seconds(TIMER_INTERVAL));
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
