#include "database.hpp"
#include "serializer.hpp"

#include <sstream>

#include <boost/log/trivial.hpp>
#include <type_traits>
#include <variant>

namespace database {
Database::Database(std::string databaseName, std::string userName,
                   std::string dbPassword, std::string host, uint port) {
  std::stringstream connBuilder;
  connBuilder << "user=" << userName << " password=" << dbPassword
              << " host=" << host << " port=" << port
              << " dbname=" << databaseName;
  dbConnection_ = pqxx::connection(connBuilder.str());
}

void Database::insert(std::string_view tableName, RowFields fields) {
  std::stringstream keys;
  std::stringstream values;
  bool first = true;
  for (auto &&[key, value] : fields) {
    if (not first) {
      keys << ", ";
      values << ", ";
    }
    keys << key;
    values << value;
    first = false;
  }
  std::stringstream queryBuilder;
  queryBuilder << "INSERT INTO " << tableName << "(" << keys.str() << ")"
               << " VALUES " << "(" << values.str() << ")";
  pqxx::work worker(dbConnection_);
  auto query = queryBuilder.str();
  BOOST_LOG_TRIVIAL(info) << "Выполняю запрос к БД: " << query;
  worker.exec(std::move(query));
  worker.commit();
}

namespace {
constexpr uint kVarCharType = 1043;
Field fromOid(const pqxx::field &field) {
  BOOST_LOG_TRIVIAL(info) << "Type OId: " << field.type();
  if (field.type() == kVarCharType) {
    return Field(field.as<std::string>());
  }
  return std::monostate();
}
} // namespace

RowFields Database::fetchSingle(std::string_view query) {
  pqxx::work worker(dbConnection_);
  auto rows = worker.exec(query);
  assert(rows.size() <= 1);
  const pqxx::row &row = rows.front();
  RowFields fields;
  for (const pqxx::field &col : row) {
    const char *name = col.name();
    if (col.is_null()) {
      fields[name] = std::monostate();
      continue;
    }
    fields[name] = fromOid(col);
  }
  return fields;
}
} // namespace database
