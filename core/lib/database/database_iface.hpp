#pragma once

#include <vector>

#include "serializer.hpp"
#include "query_builder.hpp"

namespace database {
struct AbstractDatabase {
  virtual size_t executeCommand(Query query) = 0;
  virtual RowFields fetchSingle(Query query) = 0;
  virtual std::vector<RowFields> fetchMultiple(Query query) = 0;
};
} // namespace database