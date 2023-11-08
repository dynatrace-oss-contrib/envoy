#include "source/extensions/tracers/opentelemetry/opentelemetry_tracer_impl.h"

#include <string>

#include "envoy/config/trace/v3/opentelemetry.pb.h"

#include "source/common/common/empty_string.h"
#include "source/common/common/logger.h"
#include "source/common/config/utility.h"
#include "source/common/tracing/http_tracer_impl.h"
#include "source/extensions/tracers/opentelemetry/grpc_trace_exporter.h"
#include "source/extensions/tracers/opentelemetry/http_trace_exporter.h"
#include "source/extensions/tracers/opentelemetry/resource_detectors/resource_detector.h"
#include "source/extensions/tracers/opentelemetry/resource_detectors/resource_provider.h"
#include "source/extensions/tracers/opentelemetry/samplers/sampler.h"
#include "source/extensions/tracers/opentelemetry/span_context.h"
#include "source/extensions/tracers/opentelemetry/span_context_extractor.h"
#include "source/extensions/tracers/opentelemetry/trace_exporter.h"
#include "source/extensions/tracers/opentelemetry/tracer.h"

#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

namespace {

SamplerSharedPtr
tryCreateSamper(const envoy::config::trace::v3::OpenTelemetryConfig& opentelemetry_config,
                Server::Configuration::TracerFactoryContext& context) {
  SamplerSharedPtr sampler;
  if (opentelemetry_config.has_sampler()) {
    auto& sampler_config = opentelemetry_config.sampler();
    auto* factory = Envoy::Config::Utility::getFactory<SamplerFactory>(sampler_config);
    if (!factory) {
      throw EnvoyException(fmt::format("Sampler factory not found: '{}'", sampler_config.name()));
    }
    sampler = factory->createSampler(sampler_config.typed_config(), context);
  }
  return sampler;
}

OTelSpanAttributes getInitialAttributes(const Tracing::TraceContext& trace_context,
                                        OTelSpanKind span_kind) {
  OTelSpanAttributes attributes;

  // required attributes for a server span are defined in
  // https://github.com/open-telemetry/semantic-conventions/blob/main/docs/http/http-spans.md
  if (span_kind == ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER) {
    std::vector<absl::string_view> address_port =
        absl::StrSplit(trace_context.host(), ":", absl::SkipEmpty());
    if (address_port.size() > 0) {
      attributes["server.address"] = address_port[0];
    }
    if (address_port.size() > 1) {
      attributes["server.port"] = address_port[1];
    }
    std::vector<absl::string_view> path_query =
        absl::StrSplit(trace_context.path(), "?", absl::SkipEmpty());
    if (path_query.size() > 0) {
      attributes["url.path"] = path_query[0];
    }
    if (path_query.size() > 1) {
      attributes["url.query"] = path_query[1];
    }
    auto scheme = trace_context.getByKey(":scheme");
    if (scheme.has_value()) {
      attributes["url.scheme"] = scheme.value();
    }
    if (!trace_context.method().empty()) {
      attributes["http.request.method"] = trace_context.method();
    }
  }
  return attributes;
}

OTelSpanKind getSpanKind(const Tracing::Config& config) {
  OTelSpanKind span_kind;
  // If this is downstream span that be created by 'startSpan' for downstream request, then
  // set the span type based on the spawnUpstreamSpan flag and traffic direction:
  // * If separate tracing span will be created for upstream request, then set span type to
  //   SERVER because the downstream span should be server span in trace chain.
  // * If separate tracing span will not be created for upstream request, that means the
  //   Envoy will not be treated as independent hop in trace chain and then set span type
  //   based on the traffic direction.
  span_kind =
      (config.spawnUpstreamSpan() ? ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER
       : config.operationName() == Tracing::OperationName::Egress
           ? ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_CLIENT
           : ::opentelemetry::proto::trace::v1::Span::SPAN_KIND_SERVER);
  return span_kind;
}

} // namespace

Driver::Driver(const envoy::config::trace::v3::OpenTelemetryConfig& opentelemetry_config,
               Server::Configuration::TracerFactoryContext& context)
    : Driver(opentelemetry_config, context, ResourceProviderImpl{}) {}

Driver::Driver(const envoy::config::trace::v3::OpenTelemetryConfig& opentelemetry_config,
               Server::Configuration::TracerFactoryContext& context,
               const ResourceProvider& resource_provider)
    : tls_slot_ptr_(context.serverFactoryContext().threadLocal().allocateSlot()),
      tracing_stats_{OPENTELEMETRY_TRACER_STATS(
          POOL_COUNTER_PREFIX(context.serverFactoryContext().scope(), "tracing.opentelemetry"))} {
  auto& factory_context = context.serverFactoryContext();

  Resource resource = resource_provider.getResource(opentelemetry_config, context);
  ResourceConstSharedPtr resource_ptr = std::make_shared<Resource>(std::move(resource));

  if (opentelemetry_config.has_grpc_service() && opentelemetry_config.has_http_service()) {
    throw EnvoyException(
        "OpenTelemetry Tracer cannot have both gRPC and HTTP exporters configured. "
        "OpenTelemetry tracer will be disabled.");
  }

  // Create the sampler if configured
  SamplerSharedPtr sampler = tryCreateSamper(opentelemetry_config, context);

  // Create the tracer in Thread Local Storage.
  tls_slot_ptr_->set([opentelemetry_config, &factory_context, this, resource_ptr,
                      sampler](Event::Dispatcher& dispatcher) {
    OpenTelemetryTraceExporterPtr exporter;
    if (opentelemetry_config.has_grpc_service()) {
      Grpc::AsyncClientFactoryPtr&& factory =
          factory_context.clusterManager().grpcAsyncClientManager().factoryForGrpcService(
              opentelemetry_config.grpc_service(), factory_context.scope(), true);
      const Grpc::RawAsyncClientSharedPtr& async_client_shared_ptr =
          factory->createUncachedRawAsyncClient();
      exporter = std::make_unique<OpenTelemetryGrpcTraceExporter>(async_client_shared_ptr);
    } else if (opentelemetry_config.has_http_service()) {
      exporter = std::make_unique<OpenTelemetryHttpTraceExporter>(
          factory_context.clusterManager(), opentelemetry_config.http_service());
    }
    TracerPtr tracer = std::make_unique<Tracer>(
        std::move(exporter), factory_context.timeSource(), factory_context.api().randomGenerator(),
        factory_context.runtime(), dispatcher, tracing_stats_, resource_ptr, sampler);
    return std::make_shared<TlsTracer>(std::move(tracer));
  });
}

Tracing::SpanPtr Driver::startSpan(const Tracing::Config& config,
                                   Tracing::TraceContext& trace_context,
                                   const StreamInfo::StreamInfo& stream_info,
                                   const std::string& operation_name,
                                   Tracing::Decision tracing_decision) {
  // Get tracer from TLS and start span.
  auto& tracer = tls_slot_ptr_->getTyped<Driver::TlsTracer>().tracer();
  SpanContextExtractor extractor(trace_context);
  auto span_kind = getSpanKind(config);
  auto initial_attributes = getInitialAttributes(trace_context, span_kind);
  if (!extractor.propagationHeaderPresent()) {
    // No propagation header, so we can create a fresh span with the given decision.
    Tracing::SpanPtr new_open_telemetry_span = tracer.startSpan(
        operation_name, stream_info.startTime(), initial_attributes, tracing_decision, span_kind);
    return new_open_telemetry_span;
  } else {
    // Try to extract the span context. If we can't, just return a null span.
    absl::StatusOr<SpanContext> span_context = extractor.extractSpanContext();
    if (span_context.ok()) {
      return tracer.startSpan(operation_name, stream_info.startTime(), initial_attributes,
                              span_context.value(), span_kind);
    } else {
      ENVOY_LOG(trace, "Unable to extract span context: ", span_context.status());
      return std::make_unique<Tracing::NullSpan>();
    }
  }
}

Driver::TlsTracer::TlsTracer(TracerPtr tracer) : tracer_(std::move(tracer)) {}

Tracer& Driver::TlsTracer::tracer() {
  ASSERT(tracer_);
  return *tracer_;
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
