#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"
#include "envoy/http/async_client.h"
#include "envoy/http/message.h"
#include "envoy/server/tracer_config.h"

#include "source/common/http/async_client_impl.h"
#include "source/common/http/async_client_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/utility.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampler_config.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplerConfigFetcher {
public:
  virtual ~SamplerConfigFetcher() = default;

  virtual const SamplerConfig& getSamplerConfig() const = 0;
};

class SamplerConfigFetcherImpl : public SamplerConfigFetcher,
                                 public Logger::Loggable<Logger::Id::tracing>,
                                 public Http::AsyncClient::Callbacks {
public:
  SamplerConfigFetcherImpl(
      Server::Configuration::TracerFactoryContext& context,
      const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig&
          config);

  void onSuccess(const Http::AsyncClient::Request& request,
                 Http::ResponseMessagePtr&& response) override;

  void onFailure(const Http::AsyncClient::Request& request,
                 Http::AsyncClient::FailureReason reason) override;
  void onBeforeFinalizeUpstreamSpan(Envoy::Tracing::Span& /*span*/,
                                    const Http::ResponseHeaderMap* /*response_headers*/) override{};

  const SamplerConfig& getSamplerConfig() const override { return sampler_config_; }

  ~SamplerConfigFetcherImpl() override;

private:
  Event::TimerPtr timer_;
  Upstream::ClusterManager& cluster_manager_;
  envoy::config::core::v3::HttpUri http_uri_;
  std::pair<const Http::LowerCaseString, const std::string> parsed_authorization_header_to_add_;
  Http::AsyncClient::Request* active_request_{};
  SamplerConfig sampler_config_;

  void onRequestDone();
};

using SamplerConfigFetcherPtr = std::unique_ptr<SamplerConfigFetcher>;

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
