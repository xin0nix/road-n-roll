#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <boost/url/grammar.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "router.hpp"
#include "server_iface.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

namespace router = boost::urls::router;
using tcp = asio::ip::tcp;

namespace core {
/**
 * @brief Класс HTTP-сервера, обрабатывающий POST/GET/PUT/DELETE.
 */
struct CoreServer final : AbstractServer,
                          std::enable_shared_from_this<CoreServer> {

  CoreServer() : workGuard_(boost::asio::make_work_guard(ioc_)) {}
  using Request = http::request<http::string_body>;
  using Response = http::response<http::string_body>;
  using Handler = std::function<std::optional<Response>(
      Request request, router::MatchesStorage matches)>;

  void get(std::string_view route, Handler handler) override;
  void put(std::string_view route, Handler handler) override;
  void post(std::string_view route, Handler handler) override;
  void del(std::string_view route, Handler handler) override;

  void run(tcp::endpoint endpoint) override;

protected:
  /**
   * @brief Принимает входящие соединения и запускает сессии
   *
   * @return asio::awaitable<void>
   */
  asio::awaitable<void> listenTo(tcp::endpoint endpoint);

  /**
   * @brief Корутина, обрабатывающая одну клиентскую сессию
   *
   * @param socket Уникальный сокет для клиентского соединения
   * @return asio::awaitable<void>
   */
  asio::awaitable<void> session(tcp::socket socket);

  /**
   * @brief Обрабатывает HTTP-запрос
   *
   * @param req Входящий HTTP-запрос
   * @return http::response<http::string_body> HTTP-ответ
   */
  http::response<http::string_body>
  handle_request(const http::request<http::string_body> &req);

  router::Router<Handler> routerGet_;
  router::Router<Handler> routerPut_;
  router::Router<Handler> routerPost_;
  router::Router<Handler> routerDelete_;

private:
  asio::io_context ioc_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      workGuard_;
};
} // namespace core