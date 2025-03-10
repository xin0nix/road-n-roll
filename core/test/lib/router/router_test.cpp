#include <exception>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "router.hpp"

using namespace boost::urls::router;

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