#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/sampling_controller.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(SamplingControllerTest, TestEmpty) { SamplingController sc; }

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
