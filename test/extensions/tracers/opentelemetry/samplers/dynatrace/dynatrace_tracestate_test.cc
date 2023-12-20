#include <string>

#include "source/extensions/tracers/opentelemetry/samplers/dynatrace/dynatrace_tracestate.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace Tracers {
namespace OpenTelemetry {

TEST(DynatraceTracestateTest, TestKey) {
  DtTracestateEntry tracestate("98812ad49", "980df25c");
  EXPECT_STREQ(tracestate.getKey().c_str(), "98812ad49-980df25c@dt");
}

// Test creating an invalid tag
TEST(FW4TagTest, TestInvalid) {
  auto tag = FW4Tag::createInvalid();
  EXPECT_FALSE(tag.isValid());
}

// Test creating a tag via values
TEST(FW4TagTest, TestCreateFromValues) {
  auto tag = FW4Tag::create(false, 2, 235);
  EXPECT_TRUE(tag.isValid());
  EXPECT_FALSE(tag.isIgnored());
  EXPECT_EQ(tag.getSamplingExponent(), 2);
  EXPECT_EQ(tag.getPathInfo(), 235);
  EXPECT_STREQ(tag.asString().c_str(), "fw4;0;0;0;0;0;2;eb");

  tag = FW4Tag::create(true, 8, 34566);
  EXPECT_TRUE(tag.isValid());
  EXPECT_TRUE(tag.isIgnored());
  EXPECT_EQ(tag.getSamplingExponent(), 8);
  EXPECT_EQ(tag.getPathInfo(), 34566);
  EXPECT_STREQ(tag.asString().c_str(), "fw4;0;0;0;0;1;8;8706");
}

// Test creating a tag from a string
TEST(FW4TagTest, TestCreateFromString) {
  auto tag = FW4Tag::create("fw4;0;0;0;0;0;4;1a");
  EXPECT_TRUE(tag.isValid());
  EXPECT_FALSE(tag.isIgnored());
  EXPECT_EQ(tag.getSamplingExponent(), 4);
  EXPECT_EQ(tag.getPathInfo(), 26);

  tag = FW4Tag::create("fw4;0;0;0;0;1;2;1a2b");
  EXPECT_TRUE(tag.isValid());
  EXPECT_TRUE(tag.isIgnored());
  EXPECT_EQ(tag.getSamplingExponent(), 2);
  EXPECT_EQ(tag.getPathInfo(), 6699);

  tag = FW4Tag::create("fw4;6;b3ed5cbd;0;0;0;0;28e;dd24;2h01;3hb3ed5cbd;4h00;5h01;"
                       "6he929c846864f0b96d59757ad34881a2b;7h032e812f5a9623aa");
  EXPECT_TRUE(tag.isValid());
  EXPECT_FALSE(tag.isIgnored());
  EXPECT_EQ(tag.getSamplingExponent(), 0);
  EXPECT_EQ(tag.getPathInfo(), 654);

  tag = FW4Tag::create("");
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create(";");
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create("fw4;;;;;;;;");
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create("fw4;0;0;0;0;1;2");
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create("fw4;0;0;0;0;1;2;asdf"); // invalid path info (not a hex number)
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create("fw4;0;0;0;0;1;X;a2"); // invalid exponent (not a hex number)
  EXPECT_FALSE(tag.isValid());
  tag = FW4Tag::create("fw3;0;0;0;0;0;0;a2");
  EXPECT_FALSE(tag.isValid());
}

} // namespace OpenTelemetry
} // namespace Tracers
} // namespace Extensions
} // namespace Envoy
