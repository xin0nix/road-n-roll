#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <unordered_map>

#include "router.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

/**
 * @brief Класс HTTP-сервера, обрабатывающий запросы на работу с играми.
 */
class CoreServer {
public:
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
    asio::signal_set signals(ioc_, SIGINT, SIGTERM);
    signals.async_wait([this](auto, auto) {
      ioc_.stop();
      // Можно добавить дополнительную логику очистки
    });

    asio::co_spawn(ioc_, listener({address_, port_}),
                   [](std::exception_ptr ep) {
                     if (!ep)
                       return;
                     try {
                       std::rethrow_exception(ep);
                     } catch (const std::system_error &e) {
                       std::cerr << "Системная ошибка: " << e.what()
                                 << " (code: " << e.code() << ")\n";
                     } catch (const std::exception &e) {
                       std::cerr << "Критическая ошибка: " << e.what() << "\n";
                     }
                   });

    ioc_.run();
  }

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
    try {
      auto &&executor = co_await asio::this_coro::executor;
      auto acceptor =
          asio::use_awaitable.as_default_on(tcp::acceptor(executor));
      std::cout << "Слушаю клиентов http://" << endpoint << std::endl;
      acceptor.open(endpoint.protocol());
      acceptor.bind(endpoint);
      acceptor.listen(asio::socket_base::max_listen_connections);
      for (;;) {
        asio::co_spawn(executor, session(co_await acceptor.async_accept()),
                       asio::detached);
      }
    } catch (...) {
      std::rethrow_exception(std::current_exception());
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

/**
 * @brief Главная функция для запуска HTTP-сервера
 *
 * @return int Код завершения
 */
int main() {
  try {
    CoreServer server("127.0.0.1", 8080);
    server.run();
  } catch (const std::exception &e) {
    std::cerr << "Ошибка: " << e.what() << std::endl;
    return 1;
  }
  std::cerr << "Выход" << std::endl;
  return 0;
}