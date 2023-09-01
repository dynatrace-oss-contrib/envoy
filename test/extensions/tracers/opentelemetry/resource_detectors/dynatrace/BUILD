load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_extension_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    extension_names = ["envoy.tracers.opentelemetry.resource_detectors.dynatrace"],
    deps = [
        "//envoy/registry",
        "//source/extensions/tracers/opentelemetry/resource_detectors/dynatrace:config",
        "//source/extensions/tracers/opentelemetry/resource_detectors/dynatrace:dynatrace_resource_detector_lib",
        "//test/mocks/server:tracer_factory_context_mocks",
        "//test/test_common:utility_lib",
    ],
)

envoy_extension_cc_test(
    name = "dynatrace_resource_detector_test",
    srcs = ["dynatrace_resource_detector_test.cc"],
    extension_names = ["envoy.tracers.opentelemetry.resource_detectors.dynatrace"],
    deps = [
        "//source/extensions/tracers/opentelemetry/resource_detectors/dynatrace:dynatrace_resource_detector_lib",
        "//test/mocks/server:tracer_factory_context_mocks",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/tracers/opentelemetry/resource_detectors/v3:pkg_cc_proto",
    ],
)
