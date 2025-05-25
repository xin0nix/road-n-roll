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

#include "game_store.hpp"
#include "server.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

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

    std::shared_ptr<core::AbstractServer> server =
        std::make_shared<core::CoreServer>();
    core::GameStore games;
    games.attachTo(server);
    server->run({asio::ip::make_address(host), port});
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(fatal) << "[MAIN] Ошибка: " << e.what() << std::endl;
    return 1;
  }

  BOOST_LOG_TRIVIAL(info) << "[MAIN] Завершение приложения." << std::endl;
  return 0;
}
