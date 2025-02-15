#include "matches.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace boost::urls;
using namespace boost;

struct MatchesTest : ::testing::Test {
protected:
  matches matches_;
};

TEST_F(MatchesTest, DefaultConstructor) {
  EXPECT_TRUE(matches_.empty());
  EXPECT_EQ(matches_.size(), 0);
}

TEST_F(MatchesTest, ResizeAndAccess) {
  EXPECT_THROW(matches_.at(0), std::exception);
  EXPECT_THROW(matches_.at(1), std::exception);
  matches_.resize(2);
  EXPECT_EQ(matches_.size(), 2);

  matches_.matches()[0] = "match1";
  matches_.matches()[1] = "match2";
  matches_.ids()[0] = "id1";
  matches_.ids()[1] = "id2";

  EXPECT_EQ(matches_.at(0), "match1");
  EXPECT_EQ(matches_.at(1), "match2");
  EXPECT_EQ(matches_.at("id1"), "match1");
  EXPECT_EQ(matches_.at("id2"), "match2");

  EXPECT_EQ(matches_[0], "match1");
  EXPECT_EQ(matches_[1], "match2");
  EXPECT_EQ(matches_["id1"], "match1");
  EXPECT_EQ(matches_["id2"], "match2");
}

TEST_F(MatchesTest, Find) {
  matches_.resize(2);
  EXPECT_EQ(matches_.size(), 2);

  matches_.matches()[0] = "match1";
  matches_.matches()[1] = "match2";
  matches_.ids()[0] = "id1";
  matches_.ids()[1] = "id2";

  auto it = matches_.find("id1");
  EXPECT_NE(it, matches_.end());
  EXPECT_EQ(*it, "match1");

  it = matches_.find("nonexistent");
  EXPECT_EQ(it, matches_.end());
}

TEST_F(MatchesTest, IteratorAccess) {
  matches_.resize(2);
  EXPECT_EQ(matches_.size(), 2);

  matches_.matches()[0] = "match1";
  matches_.matches()[1] = "match2";
  matches_.ids()[0] = "id1";
  matches_.ids()[1] = "id2";

  auto it = matches_.begin();
  EXPECT_EQ(*it, "match1");
  ++it;
  EXPECT_EQ(*it, "match2");
  ++it;
  EXPECT_EQ(it, matches_.end());
}

} // namespace
