#pragma once

#include "database_iface.hpp"
#include "server_iface.hpp"

namespace core {
struct GameStore : std::enable_shared_from_this<GameStore> {
  explicit GameStore(std::shared_ptr<database::AbstractDatabase> db)
      : db_(db) {}
  using Request = core::AbstractServer::Request;
  using Response = core::AbstractServer::Response;
  void attachTo(std::shared_ptr<core::AbstractServer> server);

private:
  // std::unordered_map<std::string, std::string> games_;
  std::shared_ptr<database::AbstractDatabase> db_;
};

} // namespace core