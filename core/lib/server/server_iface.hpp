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

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ip = asio::ip;

namespace core {
/**
 * @brief Интерфейс HTTP-сервера.
 */
struct AbstractServer {
  using MatchesStorage = std::unordered_map<std::string_view, std::string>;
  using Request = http::request<http::string_body>;
  using Response = http::response<http::string_body>;
  using Handler = std::function<std::optional<Response>(
      Request request, MatchesStorage matches)>;

  virtual void get(std::string_view route, Handler handler) = 0;
  virtual void put(std::string_view route, Handler handler) = 0;
  virtual void post(std::string_view route, Handler handler) = 0;
  virtual void del(std::string_view route, Handler handler) = 0;
  virtual void run(ip::tcp::endpoint endpoint) = 0;
};
} // namespace core