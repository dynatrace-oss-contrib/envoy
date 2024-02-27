#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config_provider.h"

#include <chrono>

#include "source/common/common/enum_to_int.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

static constexpr std::chrono::seconds INITIAL_TIMER_DURATION{10};
static constexpr std::chrono::minutes TIMER_INTERVAL{5};

SamplerConfigProviderImpl::SamplerConfigProviderImpl(
    Server::Configuration::TracerFactoryContext& context,
    const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig& config)
    : cluster_manager_(context.serverFactoryContext().clusterManager()),
      http_uri_(config.http_uri()),
      authorization_header_value_(absl::StrCat("Api-Token ", config.token())),
      sampler_config_(config.root_spans_per_minute()) {

  timer_ = context.serverFactoryContext().mainThreadDispatcher().createTimer([this]() -> void {
    const auto thread_local_cluster = cluster_manager_.getThreadLocalCluster(http_uri_.cluster());
    if (thread_local_cluster == nullptr) {
      ENVOY_LOG(error, "SamplerConfigProviderImpl failed: [cluster = {}] is not configured",
                http_uri_.cluster());
    } else {
      Http::RequestMessagePtr message = Http::Utility::prepareHeaders(http_uri_);
      message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
      message->headers().setReference(Http::CustomHeaders::get().Authorization,
                                      authorization_header_value_);
      active_request_ = thread_local_cluster->httpAsyncClient().send(
          std::move(message), *this,
          Http::AsyncClient::RequestOptions().setTimeout(std::chrono::milliseconds(6000)));
    }
  });

  timer_->enableTimer(std::chrono::seconds(INITIAL_TIMER_DURATION));
}

SamplerConfigProviderImpl::~SamplerConfigProviderImpl() {
  if (active_request_) {
    active_request_->cancel();
  }
}

void SamplerConfigProviderImpl::onSuccess(const Http::AsyncClient::Request& /*request*/,
                                          Http::ResponseMessagePtr&& http_response) {
  onRequestDone();
  const auto response_code = Http::Utility::getResponseStatus(http_response->headers());
  if (response_code == enumToInt(Http::Code::OK)) {
    ENVOY_LOG(debug, "Received sampling configuration from Dynatrace: {}",
              http_response->bodyAsString());
    sampler_config_.parse(http_response->bodyAsString());
  } else {
    ENVOY_LOG(warn, "Failed to get sampling configuration from Dynatrace: {}", response_code);
  }
}

void SamplerConfigProviderImpl::onFailure(const Http::AsyncClient::Request& /*request*/,
                                          Http::AsyncClient::FailureReason reason) {
  onRequestDone();
  ENVOY_LOG(info, "The OTLP export request failed. Reason {}", enumToInt(reason));
}

const SamplerConfig& SamplerConfigProviderImpl::getSamplerConfig() const { return sampler_config_; }

void SamplerConfigProviderImpl::onRequestDone() {
  active_request_ = nullptr;
  timer_->enableTimer(std::chrono::seconds(TIMER_INTERVAL));
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
