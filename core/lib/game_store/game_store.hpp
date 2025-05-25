#pragma once

#include "server_iface.hpp"

namespace core {
struct GameStore : std::enable_shared_from_this<GameStore> {
  using Request = core::AbstractServer::Request;
  using Response = core::AbstractServer::Response;
  void attachTo(std::shared_ptr<core::AbstractServer> server);

private:
  std::unordered_map<std::string, std::string> games_;
};

} // namespace core