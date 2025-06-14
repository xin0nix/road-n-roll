#include "database.hpp"
#include "serializer.hpp"

#include <boost/log/trivial.hpp>
#include <boost/uuid/string_generator.hpp>
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

size_t Database::executeCommand(Query query) {
  pqxx::work worker(dbConnection_);
  BOOST_LOG_TRIVIAL(info) << "Выполняю команду: " << query.sql;
  auto result = worker.exec(query.sql, query.params).affected_rows();
  BOOST_LOG_TRIVIAL(info) << "Затронуто строк: " << result;
  worker.commit();
  return result;
}

namespace {
constexpr uint kVarCharType = 1043;
constexpr uint kUuid = 2950;
Field fromOid(const pqxx::field &field) {
  BOOST_LOG_TRIVIAL(info) << "Type OId: " << field.type();
  if (field.type() == kVarCharType) {
    return Field(field.as<std::string>());
  }
  if (field.type() == kUuid) {
    // Convert PostgreSQL UUID string to Boost UUID type
    const std::string uuidStr = field.as<std::string>();
    boost::uuids::string_generator gen;
    boost::uuids::uuid uuid = gen(uuidStr);
    return Field(std::move(uuid));
  }
  return std::monostate();
}
} // namespace

RowFields Database::fetchSingle(Query query) {
  pqxx::work worker(dbConnection_);
  BOOST_LOG_TRIVIAL(info) << "Выполняю запрос одного элемента: " << query.sql;
  auto rows = worker.exec(query.sql, query.params);
  BOOST_LOG_TRIVIAL(info) << "Получено строк: " << rows.size();
  assert(rows.size() <= 1);
  if (rows.empty()) {
    return {};
  }
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

std::vector<RowFields> Database::fetchMultiple(Query query) {
  pqxx::work worker(dbConnection_);
  BOOST_LOG_TRIVIAL(info) << "Выполняю нескольких элементов: " << query.sql;
  auto rows = worker.exec(query.sql);
  BOOST_LOG_TRIVIAL(info) << "Получено строк: " << rows.size();
  std::vector<RowFields> result;
  for (const pqxx::row &row : rows) {
    RowFields fields;
    for (const pqxx::field &col : row) {
      const char *name = col.name();
      if (col.is_null()) {
        fields[name] = std::monostate();
        continue;
      }
      fields[name] = fromOid(col);
    }
    result.push_back(std::move(fields));
  }
  return result;
}
} // namespace database
