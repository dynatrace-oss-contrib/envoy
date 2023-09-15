#pragma once

#include <stddef.h>
#include <stdint.h>

// NOLINT(namespace-envoy)
uint64_t murmurHash264A(void const* data, size_t nbytes, uint64_t seed = 0xe17a1465);
