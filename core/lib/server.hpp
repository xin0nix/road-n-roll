#pragma once

#include "boost/url/grammar/parse.hpp"
#include "boost/url/rfc/uri_rule.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <boost/url/grammar.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <unordered_map>

namespace core {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;

using tcp = asio::ip::tcp;

struct CoreServer {
  /**
   * @brief Конструктор нового объекта http_server
   *
   * @param address Адрес для прослушивания
   * @param port Номер порта для прослушивания
   */
  CoreServer(std::string_view address, uint16_t port)
      : address_(asio::ip::make_address(address)), port_(port), ioc_(),
        workGuard_(boost::asio::make_work_guard(ioc_)), games_() {}

  void run() {
    asio::co_spawn(ioc_, listener({address_, port_}), asio::detached);
    ioc_.run();
    std::cout << "Сервер запущен на http://" << address_ << ":" << port_
              << std::endl;
  }

  // FIXME: нужно добавить router
  // https://www.boost.org/doc/libs/1_87_0/doc/antora/url/examples/router.html
  void get(std::string_view resource) {}

private:
  asio::ip::address address_;
  uint16_t port_;
  asio::io_context ioc_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      workGuard_;
  std::unordered_map<std::string, std::string> games_;

  /**
   * @brief Принимает входящие соединения и запускает сессии
   *
   * @return asio::awaitable<void>
   */
  asio::awaitable<void> listener(tcp::endpoint endpoint) {
    auto &&executor = co_await asio::this_coro::executor;
    auto acceptor = asio::use_awaitable.as_default_on(tcp::acceptor(executor));
    // acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen(asio::socket_base::max_listen_connections);
    for (;;) {
      try {
        asio::co_spawn(executor, session(co_await acceptor.async_accept()),
                       asio::detached);
      } catch (std::exception &e) {
        std::cerr << "Ошибка приема: " << e.what() << std::endl;
      }
    }
  }

  /**
   * @brief Корутина, обрабатывающая одну клиентскую сессию
   *
   * @param socket Уникальный сокет для клиентского соединения
   * @return asio::awaitable<void>
   */
  asio::awaitable<void> session(tcp::socket socket) {
    try {
      // Буфер будет хранить данные между итерациями
      beast::flat_buffer buffer;
      auto stream = asio::use_awaitable.as_default_on(
          beast::tcp_stream(std::move(socket)));
      for (;;) {
        stream.expires_after(std::chrono::seconds(30));
        http::request<http::string_body> req;
        co_await http::async_read(stream, buffer, req);
        auto res = handle_request(req);
        co_await http::async_write(stream, res, asio::use_awaitable);
        if (res.need_eof()) {
          break;
        }
      }

    } catch (std::exception &e) {
      std::cerr << "Ошибка сессии: " << e.what() << std::endl;
    }
    try {
      socket.shutdown(tcp::socket::shutdown_send);
    } catch (...) {
    }
  }

  /**
   * @brief Обрабатывает HTTP-запрос
   *
   * @param req Входящий HTTP-запрос
   * @return http::response<http::string_body> HTTP-ответ
   */
  http::response<http::string_body>
  handle_request(const http::request<http::string_body> &req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, "Core");
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    if (req.method() == http::verb::post and req.target() == "/games/") {
      return create_game(res);
    }
    res.result(http::status::not_found);
    res.set(http::field::content_type, "application/json");
    res.body() = "{}";
    return res;
  }

  /**
   * @brief Создает новую игру и возвращает ответ
   *
   * @param res Объект HTTP-ответа для заполнения
   * @return http::response<http::string_body> Заполненный HTTP-ответ
   */
  http::response<http::string_body>
  create_game(http::response<http::string_body> &res) {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string uuid_str = boost::uuids::to_string(uuid);
    std::string url = "/games/" + uuid_str + "/";
    games_[uuid_str] = "Активна";
    json::object response;
    response["url"] = url;
    res.result(http::status::created);
    res.body() = json::serialize(response);
    return res;
  }
};
} // namespace core