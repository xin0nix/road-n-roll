#include "game_store.hpp"
#include "database.hpp"
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
        database::RowFields fields{{"status_id", int(1)}};
        db_->insert("games", std::move(fields));
        std::string url = "/games/" + gameId;
        this->games_[gameId] = "Активна";
        json::object response;
        response["url"] = url;
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.result(http::status::created);
        res.body() = json::serialize(response);
        BOOST_LOG_TRIVIAL(info)
            << "[API] Создана новая игра с id: " << gameId << std::endl;
        return res;
      });
  server->get("/games/{gameId}",
              [this](Request req, auto matches) -> std::optional<Response> {
                auto &&gameId = matches.at("gameId");
                auto it = games_.find(gameId);
                if (it == games_.cend()) {
                  BOOST_LOG_TRIVIAL(info) << "[API] Игра с id " << gameId
                                          << " не найдена." << std::endl;
                  http::response<http::string_body> res{http::status::not_found,
                                                        req.version()};
                  return res;
                }

                http::response<http::string_body> res{http::status::ok,
                                                      req.version()};
                json::object response{{"url", "/games/" + gameId},
                                      {"status", it->second}};
                res.body() = json::serialize(response);
                BOOST_LOG_TRIVIAL(info)
                    << "[API] Запрошен статус игры: " << gameId << std::endl;
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