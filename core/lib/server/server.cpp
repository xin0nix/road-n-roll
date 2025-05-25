#include "server.hpp"

#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/uri_rule.hpp>

#include <boost/log/trivial.hpp>

#include <exception>
#include <iostream>
#include <unordered_map>

namespace core {
void CoreServer::get(std::string_view route, Handler handler) {
  routerGet_.insert(route, std::move(handler));
}

void CoreServer::put(std::string_view route, Handler handler) {
  routerPut_.insert(route, std::move(handler));
}

void CoreServer::post(std::string_view route, Handler handler) {
  routerPost_.insert(route, handler);
}

void CoreServer::del(std::string_view route, Handler handler) {
  routerDelete_.insert(route, handler);
}

asio::awaitable<void> CoreServer::listenTo(tcp::endpoint endpoint) {
  try {
    auto &&executor = co_await asio::this_coro::executor;
    auto acceptor = asio::use_awaitable.as_default_on(tcp::acceptor(executor));
    BOOST_LOG_TRIVIAL(info) << "[Сервер] Слушаю клиентов по адресу http://"
                            << endpoint << std::endl;
    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(asio::socket_base::max_listen_connections);
    for (;;) {
      asio::co_spawn(executor, session(co_await acceptor.async_accept()),
                     asio::detached);
    }
  } catch (std::exception &e) {
    BOOST_LOG_TRIVIAL(error)
        << "[Сессия] Ошибка в listener: " << e.what() << std::endl;
  }
}

asio::awaitable<void> CoreServer::session(tcp::socket socket) {
  BOOST_LOG_TRIVIAL(info) << "[Сессия] Новое соединение установлено."
                          << std::endl;
  try {
    // Буфер будет хранить данные между итерациями
    beast::flat_buffer buffer;
    auto stream =
        asio::use_awaitable.as_default_on(beast::tcp_stream(std::move(socket)));
    for (;;) {
      stream.expires_after(std::chrono::seconds(30));
      http::request<http::string_body> req;
      co_await http::async_read(stream, buffer, req);
      auto res = handle_request(req);
      co_await http::async_write(stream, res, asio::use_awaitable);
      if (res.need_eof()) {
        // Корректно закрываем соединение
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_send);
        // Не выводите ошибку, если ec == not_connected
        break;
      }
    }

  } catch (std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << "[Сессия] Ошибка: " << e.what() << std::endl;
  }
}

void CoreServer::run(tcp::endpoint endpoint) {
  BOOST_LOG_TRIVIAL(info) << "[Сервер] Запуск сервера..." << std::endl;
  asio::signal_set signals(ioc_, SIGINT, SIGTERM);
  signals.async_wait([this](auto, auto) {
    BOOST_LOG_TRIVIAL(info)
        << "[Сервер] Получен сигнал завершения. Остановка..." << std::endl;
    ioc_.stop();
    // Можно добавить дополнительную логику очистки
  });

  asio::co_spawn(ioc_, listenTo(endpoint), [](std::exception_ptr ep) {
    if (!ep)
      return;
    try {
      std::rethrow_exception(ep);
    } catch (const std::system_error &e) {
      BOOST_LOG_TRIVIAL(error) << "[Сервер] Системная ошибка: " << e.what()
                               << " (code: " << e.code() << ")\n";
    } catch (const std::exception &e) {
      BOOST_LOG_TRIVIAL(fatal)
          << "[Сервер] Критическая ошибка: " << e.what() << "\n";
    }
  });

  ioc_.run();
  BOOST_LOG_TRIVIAL(info) << "[Сервер] Сервер завершил работу." << std::endl;
}

http::response<http::string_body>
CoreServer::handle_request(const http::request<http::string_body> &req) {
  BOOST_LOG_TRIVIAL(info) << "[handle_request] Обработка запроса: "
                          << req.method_string() << " " << req.target()
                          << std::endl;
  http::response<http::string_body> res{http::status::ok, req.version()};
  res.set(http::field::server, "Core");
  res.set(http::field::content_type, "application/json");
  res.keep_alive(req.keep_alive());
  router::MatchesStorage matches;
  auto method = req.method();
  router::Router<Handler> *router = nullptr;
  switch (method) {
  case http::verb::get:
    router = &routerGet_;
    break;
  case http::verb::put:
    router = &routerPut_;
    break;
  case http::verb::post:
    router = &routerPost_;
    break;
  case http::verb::delete_:
    router = &routerDelete_;
    break;
  default:
    router = nullptr;
  }
  decltype(router->find({}, matches)) handler = nullptr;
  if (router) {
    handler = router->find(req.target(), matches);
  }
  if (handler) {
    auto maybeResp = (*handler)(req, matches);
    if (maybeResp) {
      BOOST_LOG_TRIVIAL(info)
          << "[handle_request] Запрос обработан маршрутизатором." << std::endl;
      return *maybeResp;
    }
  }
  BOOST_LOG_TRIVIAL(info)
      << "[handle_request] Не найден обработчик для маршрута." << std::endl;
  res.result(http::status::not_found);
  res.set(http::field::content_type, "application/json");
  res.body() = "{}";
  return res;
}

} // namespace core