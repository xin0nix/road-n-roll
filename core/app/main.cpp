#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "server.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

struct GameStore {
  using Request = core::CoreServer::Request;
  using Response = core::CoreServer::Response;
  void init(core::CoreServer &server) {
    BOOST_LOG_TRIVIAL(info) << "[Сервер] Регистрация маршрутов..." << std::endl;
    // Добавим обработчики для ресурса /games
    server.get(
        "/games", [this](Request req, auto _) -> std::optional<Response> {
          json::object response;
          json::array gameList;
          for (auto &&[uuid, _] : games_) {
            json::object entry{{"url", "/games/" + uuid}};
            gameList.push_back(std::move(entry));
          }
          response["games"] = std::move(gameList);
          http::response<http::string_body> res{http::status::ok,
                                                req.version()};
          res.result(http::status::ok);
          res.body() = json::serialize(response);
          BOOST_LOG_TRIVIAL(info)
              << "[API] Получен список всех игр. Количество: " << games_.size()
              << std::endl;
          return res;
        });
    server.post(
        "/games", [this](Request req, auto _) -> std::optional<Response> {
          boost::uuids::uuid uuid = boost::uuids::random_generator()();
          std::string gameId = boost::uuids::to_string(uuid);
          std::string url = "/games/" + gameId;
          this->games_[gameId] = "Активна";
          json::object response;
          response["url"] = url;
          http::response<http::string_body> res{http::status::ok,
                                                req.version()};
          res.result(http::status::created);
          res.body() = json::serialize(response);
          BOOST_LOG_TRIVIAL(info)
              << "[API] Создана новая игра с id: " << gameId << std::endl;
          return res;
        });
    server.get("/games/{gameId}",
               [this](Request req, auto matches) -> std::optional<Response> {
                 auto &&gameId = matches.at("gameId");
                 auto it = games_.find(gameId);
                 if (it == games_.cend()) {
                   BOOST_LOG_TRIVIAL(info) << "[API] Игра с id " << gameId
                                           << " не найдена." << std::endl;
                   http::response<http::string_body> res{
                       http::status::not_found, req.version()};
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
    server.del("/games/{gameId}",
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

private:
  std::unordered_map<std::string, std::string> games_;
};

/**
 * @brief Главная функция для запуска HTTP-сервера
 *
 * @return int Код завершения
 */
int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  try {
    BOOST_LOG_TRIVIAL(info) << "[MAIN] Запуск приложения..." << std::endl;
    // Define command line options
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "Show help message")(
        "host", po::value<std::string>()->default_value("127.0.0.1"),
        "Server host address")(
        "port", po::value<boost::asio::ip::port_type>()->default_value(8080),
        "Server port number");

    // Parse command line
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // Handle help option
    if (vm.count("help")) {
      BOOST_LOG_TRIVIAL(info) << desc << std::endl;
      return EXIT_SUCCESS;
    }

    auto host = vm["host"].as<std::string>();
    auto port = vm["port"].as<boost::asio::ip::port_type>();

    BOOST_LOG_TRIVIAL(info) << "[MAIN] Параметры запуска: host=" << host
                            << ", port=" << port << std::endl;

    core::CoreServer server;
    GameStore games;
    server.run({asio::ip::make_address(host), port});
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(fatal) << "[MAIN] Ошибка: " << e.what() << std::endl;
    return 1;
  }

  BOOST_LOG_TRIVIAL(info) << "[MAIN] Завершение приложения." << std::endl;
  return 0;
}
