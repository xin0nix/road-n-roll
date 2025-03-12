
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "router.hpp"

using namespace boost::urls;
using namespace boost::urls::router;

class SegmentPatternTest : public ::testing::Test {};

// Тесты
TEST_F(SegmentPatternTest, LiteralMatch) {
  SegmentPattern pattern;
  pattern.str_ = "test";
  pattern.isLiteral_ = true;

  pct_string_view seg("test");
  EXPECT_TRUE(pattern.match(seg));
  pct_string_view seg2("anything");
  EXPECT_FALSE(pattern.match(seg2));
}

TEST_F(SegmentPatternTest, ComparisonOperators) {
  SegmentPattern a, b;
  a.str_ = "a";
  a.isLiteral_ = true;
  b.str_ = "b";
  b.isLiteral_ = true;

  EXPECT_TRUE(a == a);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a < b);
}

TEST_F(SegmentPatternTest, NonLiteralMatch) {
  SegmentPattern pattern;
  pattern.str_ = "{test}";
  pattern.isLiteral_ = false;

  pct_string_view seg("anything");
  EXPECT_TRUE(pattern.match(seg));
}

// FIXME:
TEST_F(SegmentPatternTest, IdForNonLiteral) {
  SegmentPattern pattern;
  pattern.str_ = "test";
  pattern.isLiteral_ = false;

  EXPECT_EQ(pattern.id(), "test");
}

TEST_F(SegmentPatternTest, EmptyPattern) {
  SegmentPattern pattern;
  EXPECT_TRUE(pattern.empty());
}

// FIXME:
TEST_F(SegmentPatternTest, ParseLiteral) {
  SegmentPatternRule rule;
  const char *input = "app";
  const char *end = input + std::strlen(input);

  auto result = rule.parse(input, end);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->string(), "app");
  EXPECT_TRUE(result->isLiteral());
}

// FIXME:
TEST_F(SegmentPatternTest, ParseNonLiteral) {
  SegmentPatternRule rule;
  const char *input = "{id}";
  const char *end = input + std::strlen(input);

  auto result = rule.parse(input, end);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->id(), "id");
  EXPECT_FALSE(result->isLiteral());
}

TEST_F(SegmentPatternTest, ParseEmpty) {
  SegmentPatternRule rule;
  const char *input = "";
  const char *end = input + std::strlen(input);

  auto result = rule.parse(input, end);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

// TEST_F(SegmentPatternTest, ParseInvalid) {
//   SegmentPatternRule rule;
//   const char *input = "{invalid";
//   const char *end = input + std::strlen(input);

//   auto result = rule.parse(input, end);
//   EXPECT_FALSE(result.has_value());
// }

TEST_F(SegmentPatternTest, ParseMultipleLiteralSegments) {
  std::string_view input = "/app/games/achievements";

  auto result = parse(input, kPathPatternRule);
  ASSERT_TRUE(result.has_value());

  // auto segments = *result;
  std::vector<SegmentPattern> segments(result->begin(), result->end());
  ASSERT_EQ(segments.size(), 3);
  EXPECT_EQ(segments[0].string(), "app");
  EXPECT_TRUE(segments[0].isLiteral());
  EXPECT_EQ(segments[1].string(), "games");
  EXPECT_TRUE(segments[1].isLiteral());
  EXPECT_EQ(segments[2].string(), "achievements");
  EXPECT_TRUE(segments[2].isLiteral());
}

// FIXME:
TEST_F(SegmentPatternTest, ParseMixedSegments) {
  std::string_view input = "/app/games/{playerId}/achievements";

  auto result = parse(input, kPathPatternRule);
  ASSERT_TRUE(result.has_value());

  std::vector<SegmentPattern> segments(result->begin(), result->end());
  ASSERT_EQ(segments.size(), 4);
  EXPECT_EQ(segments[0].string(), "app");
  EXPECT_TRUE(segments[0].isLiteral());
  EXPECT_EQ(segments[1].string(), "games");
  EXPECT_TRUE(segments[1].isLiteral());
  EXPECT_EQ(segments[2].id(), "playerId");
  EXPECT_FALSE(segments[2].isLiteral());
  EXPECT_EQ(segments[3].string(), "achievements");
  EXPECT_TRUE(segments[3].isLiteral());
}