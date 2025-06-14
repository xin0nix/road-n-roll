#include "database.hpp"

#include <sstream>

#include <boost/log/trivial.hpp>

namespace database {
Database::Database(std::string databaseName, std::string userName,
                   std::string dbPassword, std::string host, uint port) {
  std::stringstream connBuilder;
  connBuilder << "user=" << userName << " password=" << dbPassword
              << " host=" << host << " port=" << port
              << " dbname=" << databaseName;
  dbConnection_ = pqxx::connection(connBuilder.str());
}

void Database::insert(std::string tableName, RowFields fields) {
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
  pqxx::work w(dbConnection_);
  auto query = queryBuilder.str();
  BOOST_LOG_TRIVIAL(info) << "Выполняю запрос к БД: " << query;
  w.exec(std::move(query));
  w.commit();
}
} // namespace database
