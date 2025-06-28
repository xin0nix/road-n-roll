#pragma once

#include <pqxx/pqxx>

#include "serializer.hpp"

namespace database {
struct Query {
  std::string sql;
  pqxx::params params;
  void append(const Field &field);
};
struct QueryBuilder {
  Query generic(std::string_view query, std::vector<Field> params);
  Query insert(std::string_view tableName, RowFields fields);
};
} // namespace database