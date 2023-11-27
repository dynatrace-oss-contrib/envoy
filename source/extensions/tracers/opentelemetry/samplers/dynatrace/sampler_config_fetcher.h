#pragma once

#include <utility>
#include <vector>

#include "envoy/config/core/v3/http_service.pb.h"
#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.h"
#include "envoy/http/async_client.h"
#include "envoy/http/message.h"
#include "envoy/server/tracer_config.h"

#include "source/common/http/async_client_impl.h"
#include "source/common/http/async_client_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SamplerConfigFetcher : public Logger::Loggable<Logger::Id::tracing>,
                             public Http::AsyncClient::Callbacks {
public:
  SamplerConfigFetcher(Server::Configuration::TracerFactoryContext& context,
                       const envoy::config::core::v3::HttpService& http_service);

  void onSuccess(const Http::AsyncClient::Request& request,
                 Http::ResponseMessagePtr&& response) override;

  void onFailure(const Http::AsyncClient::Request& request,
                 Http::AsyncClient::FailureReason reason) override;
  void onBeforeFinalizeUpstreamSpan(Envoy::Tracing::Span& /*span*/,
                                    const Http::ResponseHeaderMap* /*response_headers*/) override{};

  ~SamplerConfigFetcher() override;

private:
  Event::TimerPtr timer_;
  Upstream::ClusterManager& cluster_manager_;
  envoy::config::core::v3::HttpService http_service_;
  std::vector<std::pair<const Http::LowerCaseString, const std::string>> parsed_headers_to_add_;
  Http::AsyncClient::Request* active_request_{};
};

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
