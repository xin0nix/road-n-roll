#pragma once

#include <pqxx/pqxx>

#include "database_iface.hpp"
#include "serializer.hpp"

namespace database {
struct Database final : AbstractDatabase {
  Database(std::string databaseName, std::string userName,
           std::string dbPassword, std::string host, uint port);

  size_t executeCommand(Query query) final;

  RowFields fetchSingle(Query query) final;

  std::vector<RowFields> fetchMultiple(Query query) final;

private:
  // FIXME: pimpl
  pqxx::connection dbConnection_;
};
} // namespace database