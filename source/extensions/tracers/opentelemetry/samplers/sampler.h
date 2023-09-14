#pragma once

#include <memory>
#include <string>

#include "envoy/config/typed_config.h"
#include "envoy/server/tracer_config.h"

#include "envoy/tracing/trace_context.h"

#include "opentelemetry/proto/trace/v1/trace.pb.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

class SpanContext;

enum class Decision {
  // IsRecording() == false, span will not be recorded and all events and attributes will be
  // dropped.
  DROP,
  // IsRecording() == true, but Sampled flag MUST NOT be set.
  RECORD_ONLY,
  // IsRecording() == true AND Sampled flag` MUST be set.
  RECORD_AND_SAMPLE
};

struct SamplingResult
{
  Decision decision;
  // A set of span Attributes that will also be added to the Span. Can be nullptr.
  std::unique_ptr<const std::map<std::string, std::string>> attributes;
  // The tracestate used by the span.
  std::string trace_state;

  inline bool isRecording()
  {
    return decision == Decision::RECORD_ONLY || decision == Decision::RECORD_AND_SAMPLE;
  }
  inline bool isSampled() { return decision == Decision::RECORD_AND_SAMPLE; }
};

/**
 * @brief The base type for all samplers
 *
 */
class Sampler {
public:
  virtual ~Sampler() = default;

  /**
   * @brief Decides if a trace should be sampled.
   *
   * @return a SamplingResult
   */
  virtual SamplingResult
  shouldSample(absl::StatusOr<SpanContext>& parent_context, const std::string& trace_id, const std::string& name,
               ::opentelemetry::proto::trace::v1::Span::SpanKind spankind, const std::map<std::string, std::string>& attributes) PURE;

  virtual std::string getDescription() const PURE;
};

using SamplerPtr = std::shared_ptr<Sampler>;

/*
 * A factory for creating sampler without configuration.
 */
class SamplerFactory : public Envoy::Config::TypedFactory {
public:
  ~SamplerFactory() override = default;

  /**
   * @brief Creates a sampler
   * @param message The sampler config
   * @param context The tracer factory context.
   * @return SamplerPtr
   */
  virtual SamplerPtr
  createSampler(const Protobuf::Message& message, Server::Configuration::TracerFactoryContext& context) PURE;

  std::string category() const override { return "envoy.tracers.opentelemetry.samplers"; }
};

using SamplerFactoryPtr = std::unique_ptr<SamplerFactory>;

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy