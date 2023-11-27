#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/tracestate.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

// Test parsing an empty string
TEST(TracestateTest, TestEmpty) {
  Tracestate tracestate;
  tracestate.parse("");
  EXPECT_EQ(0, tracestate.entries().size());
  EXPECT_STREQ(tracestate.asString().c_str(), "");
}

// Test parsing a tracestate string with one entry
TEST(TracestateTest, TestSingleEntry) {
  Tracestate tracestate;
  tracestate.parse("key0=value0");
  auto entries = tracestate.entries();
  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");
}

// Test parsing a tracestate string an invalid entry
TEST(TracestateTest, TestInvalidEntry) {
  Tracestate tracestate;
  tracestate.parse("key0=value0,key1=");
  auto entries = tracestate.entries();
  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");

  tracestate.parse("key0=value0,=value1");
  entries = tracestate.entries();
  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");

  tracestate.parse("key0=value0,something");
  entries = tracestate.entries();
  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");
}

// Test parsing a tracestate string with multiple entries
TEST(TracestateTest, TestMultiEntries) {
  Tracestate tracestate;
  tracestate.parse("key0=value0,key1=value1,key2=value2,key3=value3");
  auto entries = tracestate.entries();
  EXPECT_EQ(4, entries.size());

  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");

  EXPECT_STREQ(entries[1].key.c_str(), "key1");
  EXPECT_STREQ(entries[1].value.c_str(), "value1");

  EXPECT_STREQ(entries[2].key.c_str(), "key2");
  EXPECT_STREQ(entries[2].value.c_str(), "value2");

  EXPECT_STREQ(entries[3].key.c_str(), "key3");
  EXPECT_STREQ(entries[3].value.c_str(), "value3");
}

// Test parsing a tracestate string with optional white spaces
TEST(TracestateTest, TestWithOWS) {
  Tracestate tracestate;
  // whitespace before and after ',' should be removed
  // whitespace inside value is allowed
  const char* c =
      "key0=value0,key1=value1, key2=val  ue2 ,  key3=value3  ,key4=value4 , key5=value5";
  tracestate.parse(c);
  EXPECT_STREQ(tracestate.asString().c_str(), c);
  auto entries = tracestate.entries();
  EXPECT_EQ(6, entries.size());

  EXPECT_STREQ(entries[0].key.c_str(), "key0");
  EXPECT_STREQ(entries[0].value.c_str(), "value0");

  EXPECT_STREQ(entries[1].key.c_str(), "key1");
  EXPECT_STREQ(entries[1].value.c_str(), "value1");

  EXPECT_STREQ(entries[2].key.c_str(), "key2");
  EXPECT_STREQ(entries[2].value.c_str(), "val  ue2");

  EXPECT_STREQ(entries[3].key.c_str(), "key3");
  EXPECT_STREQ(entries[3].value.c_str(), "value3");

  EXPECT_STREQ(entries[4].key.c_str(), "key4");
  EXPECT_STREQ(entries[4].value.c_str(), "value4");

  EXPECT_STREQ(entries[5].key.c_str(), "key5");
  EXPECT_STREQ(entries[5].value.c_str(), "value5");
}

// Test appending to an empty tracestate
TEST(TracestateTest, TestAppendToEmpty) {
  Tracestate tracestate;
  tracestate.add("new_key", "new_value");
  auto entries = tracestate.entries();
  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ(entries[0].key.c_str(), "new_key");
  EXPECT_STREQ(entries[0].value.c_str(), "new_value");
  EXPECT_STREQ(tracestate.asString().c_str(), "new_key=new_value");
}

// Test appending to an existing tracestate
TEST(TracestateTest, TestAppend) {
  Tracestate tracestate;
  tracestate.parse("key1=value1,key2=value2,key3=value3");
  tracestate.add("new_key", "new_value");

  auto entries = tracestate.entries();
  EXPECT_EQ(4, entries.size());
  EXPECT_STREQ(tracestate.asString().c_str(),
               "new_key=new_value,key1=value1,key2=value2,key3=value3");
  EXPECT_STREQ(entries[0].key.c_str(), "new_key");
  EXPECT_STREQ(entries[0].value.c_str(), "new_value");
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
