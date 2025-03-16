#include <exception>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "boost/url/url.hpp"
#include "router.hpp"

using namespace boost::urls::router;
using namespace boost::urls;

class RouterTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Инициализация Router
    router_ = std::make_unique<Router<std::string>>();
  }

  std::unique_ptr<Router<std::string>> router_;
};

TEST_F(RouterTest, InsertRootRoute) {
  std::string resource = "cat";
  router_->insert("/root", resource);

  // Проверяем, что узел был добавлен
  ASSERT_EQ(router_->size(), 2);
  auto &node = router_->nodeAt(1);
  EXPECT_TRUE(node.seg.isLiteral());
  EXPECT_EQ(node.seg.string(), "root");

  // Проверяем, что ресурс был корректно добавлен
  ASSERT_NE(node.resource, nullptr);
  EXPECT_EQ(router_->valueAt(1), "cat");
}

TEST_F(RouterTest, InsertChildRoute) {
  std::string rootResource = "cat";
  std::string childResource = "dog";
  router_->insert("/root", rootResource);
  router_->insert("/root/child", childResource);

  // Проверяем, что узлы были добавлены
  ASSERT_EQ(router_->size(), 3);
  EXPECT_EQ(router_->nodeAt(1).seg.string(), "root");
  EXPECT_EQ(router_->nodeAt(2).seg.string(), "child");

  // Проверяем, что ресурсы были корректно добавлены
  EXPECT_EQ(router_->valueAt(1), "cat");
  EXPECT_EQ(router_->valueAt(2), "dog");
}

TEST_F(RouterTest, UpdateExistingRoute) {
  std::string resource1 = "resource1";
  std::string resource2 = "resource2";
  router_->insert("/root", resource1);
  router_->insert("/root", resource2);

  // Проверяем, что узел был обновлен
  ASSERT_EQ(router_->size(), 2);
  EXPECT_EQ(router_->nodeAt(1).seg.string(), "root");

  // Проверяем, что ресурс был обновлен
  EXPECT_EQ(router_->valueAt(1), "resource2");
}

// FIXME: поправить негативные кейсы
TEST_F(RouterTest, DISABLED_InsertWithInvalidPath) {
  std::string resource = "";
  EXPECT_THROW(router_->insert("invalid\\@#$%^&*Ipath", resource),
               std::exception);
}

// FIXME: нужно корректно обрабатывать негативные кейсы
TEST_F(RouterTest, DISABLED_NormalizePath) {
  std::string resource = "cat";
  router_->insert("///root//child///", resource);

  // Проверяем, что путь был нормализован
  ASSERT_EQ(router_->size(), 3);
  EXPECT_EQ(router_->nodeAt(1).seg.string(), "child");
  EXPECT_TRUE(router_->nodeAt(1).seg.isLiteral());

  // Проверяем, что ресурс был корректно добавлен
  EXPECT_EQ(router_->valueAt(1), "cat");
}

TEST_F(RouterTest, InsertRouteWithNonLiteralSegment) {
  std::string resource = "cat";
  router_->insert("/root/{param}", resource);

  // Проверяем, что узел с нелитеральным сегментом был добавлен
  ASSERT_EQ(router_->size(), 3);
  EXPECT_EQ(router_->nodeAt(1).seg.string(), "root");
  // Проверяем, что сегмент не литеральный
  EXPECT_FALSE(router_->nodeAt(2).seg.isLiteral());
  EXPECT_EQ(router_->nodeAt(2).seg.id(), "param");

  // Проверяем, что ресурс был корректно добавлен
  EXPECT_EQ(router_->valueAt(2), "cat");
}

TEST_F(RouterTest, MatchLiteralSegment) {
  std::string childResource = "dog";
  router_->insert("/root/child", childResource);
  boost::urls::url path("/root/child");
  MatchesStorage matches;
  auto *result = router_->find(path.encoded_segments(), matches);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(*result, "dog");
}

//                     root
//                       |
//         +-------------+-------------+
//         |             |             |
//     branch1        branch2        branch3
//     /     \           |          /       \
//  leaf1   leaf2      child      child    sibling
// (apple) (banana)      |         |         |
//                    /     \    leaf5     (fig)
//                leaf3    leaf4  (elderberry)
//               (cherry)    |
//                          deep
//                         (date)
TEST_F(RouterTest, MatchLiteralSegmentWithComplexTree) {
  // Insert multiple resources with different paths
  router_->insert("/root/branch1/leaf1", "apple");
  router_->insert("/root/branch1/leaf2", "banana");
  router_->insert("/root/branch2/child/leaf3", "cherry");
  router_->insert("/root/branch2/child/leaf4/deep", "date");
  router_->insert("/root/branch3/child/leaf5", "elderberry");
  router_->insert("/root/branch3/sibling", "fig");

  {
    SCOPED_TRACE("Test exact matches");
    boost::urls::url path1("/root/branch1/leaf1");
    boost::urls::url path2("/root/branch2/child/leaf4/deep");
    boost::urls::url path3("/root/branch3/sibling");

    MatchesStorage matches;
    auto *result1 = router_->find(path1.encoded_segments(), matches);
    auto *result2 = router_->find(path2.encoded_segments(), matches);
    auto *result3 = router_->find(path3.encoded_segments(), matches);

    EXPECT_EQ(*result1, "apple");
    EXPECT_EQ(*result2, "date");
    EXPECT_EQ(*result3, "fig");
  }

  {
    SCOPED_TRACE("Test partial matches");
    boost::urls::url path1("/root/branch1");
    boost::urls::url path2("/root/branch2/child");
    boost::urls::url path3("/root/branch3");

    MatchesStorage matches;
    auto *result1 = router_->find(path1.encoded_segments(), matches);
    auto *result2 = router_->find(path2.encoded_segments(), matches);
    auto *result3 = router_->find(path3.encoded_segments(), matches);

    EXPECT_EQ(result1, nullptr);
    EXPECT_EQ(result2, nullptr);
    EXPECT_EQ(result3, nullptr);
  }

  // Test non-matching paths
  {
    boost::urls::url path1("/root/branch1/leaf3");
    boost::urls::url path2("/root/branch2/child/leaf4/shallow");
    boost::urls::url path3("/root/branch4/newpath");

    MatchesStorage matches;
    auto *result1 = router_->find(path1.encoded_segments(), matches);
    auto *result2 = router_->find(path2.encoded_segments(), matches);
    auto *result3 = router_->find(path3.encoded_segments(), matches);

    EXPECT_EQ(result1, nullptr);
    EXPECT_EQ(result2, nullptr);
    EXPECT_EQ(result3, nullptr);
  }
}

//
//                                      app
//                                       |
//                   +----------+--------+--------+-----------+
//                   |          |        |        |           |
//                 games     tournaments users   static    (empty)
//                   |          |        |        |
//             +-----+-----+    |     {userId}   |
//             |           |    |        |      assets
//         {gameId}   (empty)   |     profile     |
//           /    \             |  (user_profile) |
//          /      \            |                 +----------+
//     players leaderboard   matches          {assetType}    |
//        | (game_leaderboard)  |                            |
//     {playerId}            {matchId}                   {assetId}
//     (player_info)       (match_details)              (asset_data)
//
TEST_F(RouterTest, MatchMixedLiteralAndReplacementFields) {
  // Insert routes with mixed literal and replacement fields
  router_->insert("/app/games/{gameId}/players/{playerId}", "player_info");
  router_->insert("/app/games/{gameId}/leaderboard", "game_leaderboard");
  router_->insert("/app/tournaments/{tournamentId}/matches/{matchId}",
                  "match_details");
  router_->insert("/app/users/{userId}/profile", "user_profile");
  router_->insert("/app/static/assets/{assetType}/{assetId}", "asset_data");

  {
    SCOPED_TRACE("Positive cases");
    {
      SCOPED_TRACE("Exact match with one replacement field");
      boost::urls::url path("/app/games/54321/leaderboard");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_NE(result, nullptr);
      EXPECT_EQ(*result, "game_leaderboard");
      EXPECT_EQ(matches.size(), 1);
      EXPECT_EQ(matches.at("gameId"), "54321");
    }

    {
      SCOPED_TRACE("Exact match with replacement fields (1)");
      boost::urls::url path("/app/games/12345/players/67890");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_NE(result, nullptr);
      EXPECT_EQ(*result, "player_info");
      EXPECT_EQ(matches.size(), 2);
      EXPECT_EQ(matches.at("gameId"), "12345");
      EXPECT_EQ(matches.at("playerId"), "67890");
    }

    {
      SCOPED_TRACE("Exact match with replacement fields (2)");
      boost::urls::url path("/app/tournaments/t123/matches/m456");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_NE(result, nullptr);
      EXPECT_EQ(*result, "match_details");
      EXPECT_EQ(matches.size(), 2);
      EXPECT_EQ(matches.at("tournamentId"), "t123");
      EXPECT_EQ(matches.at("matchId"), "m456");
    }

    {
      SCOPED_TRACE("Exact match with replacement fields (3)");
      boost::urls::url path("/app/static/assets/images/logo123");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_NE(result, nullptr);
      EXPECT_EQ(*result, "asset_data");
      EXPECT_EQ(matches.size(), 2);
      EXPECT_EQ(matches.at("assetType"), "images");
      EXPECT_EQ(matches.at("assetId"), "logo123");
    }

    {
      SCOPED_TRACE("Match with replacement field in the middle");
      boost::urls::url path("/app/users/u789/profile");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_NE(result, nullptr);
      EXPECT_EQ(*result, "user_profile");
      EXPECT_EQ(matches.size(), 1);
      EXPECT_EQ(matches.at("userId"), "u789");
    }
  }

  {
    SCOPED_TRACE("Negative cases");
    {
      SCOPED_TRACE("Non-matching path");
      boost::urls::url path("/app/invalid/path");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_EQ(result, nullptr);
      EXPECT_EQ(matches.size(), 0);
    }

    {
      SCOPED_TRACE("Partial match (should not match)");
      boost::urls::url path("/app/games/12345");
      MatchesStorage matches;
      auto *result = router_->find(path.encoded_segments(), matches);

      EXPECT_EQ(result, nullptr);
      EXPECT_EQ(matches.size(), 0);
    }
  }
}
