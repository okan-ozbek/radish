#include <csignal>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

#include "RadishDB.h"
#include "network/TcpServer.h"

namespace
{
struct ServerOptions
{
    std::string bindAddress{ "127.0.0.1" };
    std::string dataFile{ "radish" };
    std::uint16_t port{ 6379 };
    std::optional<Timestamp> ttl{};
};

ServerOptions ParseOptions(const int argumentCount, char* arguments[])
{
    ServerOptions options;

    for (int index = 1; index < argumentCount; index += 2) {
        if (index + 1 >= argumentCount) {
            throw std::invalid_argument("Missing value for " + std::string(arguments[index]));
        }

        const std::string option{ arguments[index] };
        const std::string value{ arguments[index + 1] };

        if (option == "--bind") {
            options.bindAddress = value;
        }
        else if (option == "--port") {
            const auto parsed = std::stoul(value);

            if (parsed > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument("Port must be between 0 and 65535");
            }

            options.port = static_cast<std::uint16_t>(parsed);
        }
        else if (option == "--data-file") {
            options.dataFile = value;
        }
        else if (option == "--ttl-ms") {
            options.ttl = std::stoll(value);

            if (*options.ttl < 0) {
                throw std::invalid_argument("TTL must be non-negative");
            }
        }
        else {
            throw std::invalid_argument("Unknown option: " + option);
        }
    }

    return options;
}
}

int main(int argumentCount, char* arguments[])
{
    try {
        const auto options = ParseOptions(argumentCount, arguments);
        std::unique_ptr<RadishDB<std::string>> database;

        if (options.ttl) {
            database = std::make_unique<RadishDB<std::string>>(options.dataFile, *options.ttl);
        }
        else {
            database = std::make_unique<RadishDB<std::string>>(options.dataFile);
        }

        asio::io_context context;
        radish::network::TcpServer server(context, *database, options.bindAddress, options.port);
        asio::signal_set signals(context, SIGINT, SIGTERM);

        signals.async_wait([&server, &context](const asio::error_code&, int) {
            server.Stop();
            context.stop();
        });

        server.Start();

        std::cout << "Radish listening on " << options.bindAddress << ":" << server.Port() << '\n';

        context.run();
        database->Compact();

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "radish-server: " << error.what() << '\n';

        return 1;
    }
}
