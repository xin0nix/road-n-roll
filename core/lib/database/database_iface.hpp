#pragma once

#include <vector>

#include "serializer.hpp"

namespace database {
struct AbstractDatabase {
  virtual void insert(std::string_view tableName, RowFields fields) = 0;
  virtual RowFields fetchSingle(std::string_view query) = 0;
};
} // namespace database