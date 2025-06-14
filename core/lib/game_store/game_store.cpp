#include "game_store.hpp"
#include "database.hpp"
#include "serializer.hpp"

#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <format>

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
    for (auto &&[uuid, _] : games_) {
      json::object entry{{"url", "/games/" + uuid}};
      gameList.push_back(std::move(entry));
    }
    response["games"] = std::move(gameList);
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.result(http::status::ok);
    res.body() = json::serialize(response);
    BOOST_LOG_TRIVIAL(info)
        << "[API] Получен список всех игр. Количество: " << games_.size()
        << std::endl;
    return res;
  });
  server->post(
      "/games", [this](Request req, auto _) -> std::optional<Response> {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string gameId = boost::uuids::to_string(uuid);
        database::RowFields fields{{"status_id", int(1)}, {"game_id", uuid}};
        db_->insert("games", std::move(fields));
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
        // std::string uuid = "fc3c9c0c-99e0-44f3-9b90-57df5ec87128";
        auto sql = std::format(R"sql(
                  SELECT game_statuses.status_name as status_name
                  FROM games LEFT JOIN game_statuses
                  ON games.status_id = game_statuses.status_id
                  WHERE games.game_id = '{}'::uuid;)sql",
                               gameId);
        BOOST_LOG_TRIVIAL(info) << "[API] Запрашиваю данные игры: " << sql;
        auto fields = db_->fetchSingle(sql);
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
                auto it = games_.find(gameId);

                games_.erase(it);
                http::response<http::string_body> res{http::status::no_content,
                                                      req.version()};
                BOOST_LOG_TRIVIAL(info)
                    << "[API] Удалена игра: " << gameId << std::endl;
                return res;
              });
}

} // namespace core