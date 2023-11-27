#include "config.h"

#include "envoy/extensions/tracers/opentelemetry/samplers/v3/dynatrace_sampler.pb.validate.h"

#include "source/common/config/utility.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_sampler.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

SamplerSharedPtr DynatraceSamplerFactory::createSampler(
    const Protobuf::Message& config, Server::Configuration::TracerFactoryContext& context,
    const envoy::config::trace::v3::OpenTelemetryConfig& opentelemetry_config) {
  auto mptr = Envoy::Config::Utility::translateAnyToFactoryConfig(
      dynamic_cast<const ProtobufWkt::Any&>(config), context.messageValidationVisitor(), *this);
  return std::make_shared<DynatraceSampler>(
      MessageUtil::downcastAndValidate<
          const envoy::extensions::tracers::opentelemetry::samplers::v3::DynatraceSamplerConfig&>(
          *mptr, context.messageValidationVisitor()),
      context, opentelemetry_config);
}

/**
 * Static registration for the Env sampler factory. @see RegisterFactory.
 */
REGISTER_FACTORY(DynatraceSamplerFactory, SamplerFactory);

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
