#include "query_builder.hpp"

#include <sstream>
#include <stdexcept>

namespace database {
void Query::append(const Field &field) {
  auto visitor = Overload{
      [this](std::monostate) {
        throw std::logic_error("Unexpected monostate");
      },
      [this](std::string s) { params.append(s); },
      [this](boost::uuids::uuid u) {
        params.append(boost::uuids::to_string(u));
      },
      [this](int16_t x) { params.append(x); },
      [this](int32_t x) { params.append(x); },
      [this](int64_t x) { params.append(x); },
      [this](float x) { params.append(x); },
  };
  std::visit(visitor, field);
}

Query QueryBuilder::generic(std::string_view query, std::vector<Field> params) {
  Query res;
  res.sql = query;
  for (auto &param : params) {
    res.append(param);
  }
  return res;
}

Query QueryBuilder::insert(std::string_view tableName, RowFields fields) {
  std::stringstream keys;
  std::stringstream values;
  Query result;
  int idx = 1;
  for (auto &&[key, value] : fields) {
    if (idx != 1) {
      keys << ", ";
      values << ", ";
    }
    keys << key;
    values << "$" << std::to_string(idx);
    result.append(value);
    idx++;
  }

  result.sql = std::format(R"sql(
    INSERT INTO {} ({})
    VALUES ({})
  )sql",
                             tableName, keys.str(), values.str());
  return result;
}
} // namespace database