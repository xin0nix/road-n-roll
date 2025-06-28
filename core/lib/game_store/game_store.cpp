#include "game_store.hpp"
#include "query_builder.hpp"
#include "serializer.hpp"

#include <boost/json.hpp>
#include <boost/log/trivial.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace core {

void GameStore::attachTo(std::shared_ptr<core::AbstractServer> server) {
  BOOST_LOG_TRIVIAL(info) << "[Сервер] Регистрация маршрутов..." << std::endl;
  // Добавим обработчики для ресурса /games
  server->get("/games", [this](Request req, auto _) -> std::optional<Response> {
    json::object response;
    json::array gameList;
    database::Query query;
    query.sql = R"sql(
      SELECT game_id FROM games
    )sql";
    auto rows = db_->fetchMultiple(query);
    for (auto &&row : rows) {
      auto uuidField = row["game_id"];
      auto uuid = std::get<boost::uuids::uuid>(uuidField);
      auto gameId = boost::uuids::to_string(uuid);
      json::object entry{{"url", "/games/" + gameId}};
      gameList.push_back(std::move(entry));
    }
    response["games"] = std::move(gameList);
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.result(http::status::ok);
    res.body() = json::serialize(response);
    BOOST_LOG_TRIVIAL(info)
        << "[API] Получен список всех игр. Количество: " << rows.size()
        << std::endl;
    return res;
  });
  server->post(
      "/games", [this](Request req, auto _) -> std::optional<Response> {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string gameId = boost::uuids::to_string(uuid);
        database::RowFields fields{{"status_id", int(1)}, {"game_id", uuid}};
        auto query =
            database::QueryBuilder().insert("games", std::move(fields));
        db_->executeCommand(query);
        std::string url = "/games/" + gameId;
        json::object response;
        response["url"] = url;
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.result(http::status::created);
        res.body() = json::serialize(response);
        BOOST_LOG_TRIVIAL(info)
            << "[API] Создана новая игра с id: " << gameId << std::endl;
        return res;
      });
  server->get(
      "/games/{gameId}",
      [this](Request req, auto matches) -> std::optional<Response> {
        auto &&gameId = matches.at("gameId");
        database::Query query;
        query.sql = R"sql(
                  SELECT game_statuses.status_name as status_name
                  FROM games LEFT JOIN game_statuses
                  ON games.status_id = game_statuses.status_id
                  WHERE games.game_id = $1)sql";
        query.append(gameId);
        BOOST_LOG_TRIVIAL(info)
            << "[API] Запрашиваю данные игры: " << query.sql;
        auto fields = db_->fetchSingle(query);
        if (fields.empty()) {
          BOOST_LOG_TRIVIAL(info)
              << "[API] Игра с id " << gameId << " не найдена." << std::endl;
          http::response<http::string_body> res{http::status::not_found,
                                                req.version()};
          return res;
        }

        http::response<http::string_body> res{http::status::ok, req.version()};
        auto statusName = std::get<std::string>(fields.at("status_name"));
        json::object response{{"url", "/games/" + gameId},
                              {"status", statusName}};
        res.body() = json::serialize(response);
        return res;
      });
  server->del("/games/{gameId}",
              [this](Request req, auto matches) -> std::optional<Response> {
                auto &&gameId = matches.at("gameId");
                database::Query query;
                query.sql = R"sql(
                  DELETE FROM games WHERE game_id=$1;
                )sql";
                query.append(gameId);
                auto affectedRows = db_->executeCommand(query);
                auto status = affectedRows == 1 ? http::status::no_content
                                                : http::status::not_found;
                http::response<http::string_body> res{status, req.version()};
                BOOST_LOG_TRIVIAL(info)
                    << "[API] Удалена игра: " << gameId << std::endl;
                return res;
              });
}

} // namespace core