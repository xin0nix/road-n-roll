#pragma once

#include <pqxx/pqxx>

#include "database_iface.hpp"

namespace database {
struct Database final : AbstractDatabase {
  Database(std::string databaseName, std::string userName,
           std::string dbPassword, std::string host, uint port);

  void insert(std::string tableName, RowFields fields) final;

private:
  // FIXME: pimpl
  pqxx::connection dbConnection_;
};
} // namespace database