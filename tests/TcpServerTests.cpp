#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <catch2/catch_test_macros.hpp>

#include "RadishDB.h"
#include "TestSupport.h"
#include "network/TcpServer.h"

namespace {
class RunningServer {
public:
    explicit RunningServer(const std::string& databaseName)
        : m_database{ databaseName }
        , m_server{ m_context, m_database, "127.0.0.1", 0 }
    {
        m_server.Start();
        m_thread = std::thread{ [this] {
            m_context.run();
        } };
    }

    ~RunningServer() {
        m_server.Stop();
        m_context.stop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    [[nodiscard]] unsigned short Port() const {
        return m_server.Port();
    }

private:
    asio::io_context m_context;
    RadishDB<std::string> m_database;
    radish::network::TcpServer m_server;
    std::thread m_thread;
};

asio::ip::tcp::socket Connect(asio::io_context& context, const unsigned short port) {
    asio::ip::tcp::socket socket(context);
    socket.connect({ asio::ip::make_address("127.0.0.1"), port });
    return socket;
}

std::string Request(asio::ip::tcp::socket& socket, const std::string& request, const std::size_t replySize) {
    asio::write(socket, asio::buffer(request));
    std::string response(replySize, '\0');
    asio::read(socket, asio::buffer(response));
    return response;
}
}

TEST_CASE("TCP server dispatches RESP commands over a real loopback connection", "[tcp][integration]")
{
    TemporaryDatabaseFile file{ "tcp-commands" };
    RunningServer server(file.DatabaseName());
    asio::io_context clientContext;
    auto client = Connect(clientContext, server.Port());

    REQUIRE(Request(client, "*1\r\n$4\r\nPING\r\n", 7) == "+PONG\r\n");
    REQUIRE(Request(client, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n", 5) == "+OK\r\n");
    REQUIRE(Request(client, "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n", 11) == "$5\r\nvalue\r\n");
    REQUIRE(Request(client, "*2\r\n$6\r\nEXISTS\r\n$3\r\nkey\r\n", 4) == ":1\r\n");
    REQUIRE(Request(client, "*2\r\n$3\r\nDEL\r\n$3\r\nkey\r\n", 4) == ":1\r\n");
}

TEST_CASE("TCP server processes pipelined requests and RESP3 negotiation", "[tcp][integration][resp3]")
{
    TemporaryDatabaseFile file{ "tcp-pipelining" };
    RunningServer server(file.DatabaseName());
    asio::io_context clientContext;
    auto client = Connect(clientContext, server.Port());

    const std::string pipelinedRequest{ "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPING\r\n" };
    asio::write(client, asio::buffer(pipelinedRequest));
    std::array<char, 14> pongs{};
    asio::read(client, asio::buffer(pongs));
    REQUIRE(std::string_view(pongs.data(), pongs.size()) == "+PONG\r\n+PONG\r\n");

    const std::string helloRequest{ "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n" };
    asio::write(client, asio::buffer(helloRequest));
    std::string helloResponse;
    asio::read_until(client, asio::dynamic_buffer(helloResponse), "\r\n");
    INFO(helloResponse);
    REQUIRE(helloResponse.starts_with("%6\r\n"));
}

TEST_CASE("TCP server serves simultaneous clients and rejects malformed requests", "[tcp][integration][failure]")
{
    TemporaryDatabaseFile file{ "tcp-clients" };
    RunningServer server(file.DatabaseName());
    asio::io_context firstContext;
    asio::io_context secondContext;
    auto first = Connect(firstContext, server.Port());
    auto second = Connect(secondContext, server.Port());

    REQUIRE(Request(first, "*1\r\n$4\r\nPING\r\n", 7) == "+PONG\r\n");
    REQUIRE(Request(second, "*1\r\n$4\r\nPING\r\n", 7) == "+PONG\r\n");

    asio::write(first, asio::buffer("+PING\r\n"));
    std::array<char, 5> errorPrefix{};
    asio::read(first, asio::buffer(errorPrefix));
    REQUIRE(std::string_view(errorPrefix.data(), errorPrefix.size()) == "-ERR ");
}

TEST_CASE("TCP server restores persisted values after restart", "[tcp][integration][persistence]")
{
    TemporaryDatabaseFile file{ "tcp-restart" };
    {
        RunningServer server(file.DatabaseName());
        asio::io_context clientContext;
        auto client = Connect(clientContext, server.Port());
        REQUIRE(Request(client, "*3\r\n$3\r\nSET\r\n$7\r\npersist\r\n$5\r\nvalue\r\n", 5) == "+OK\r\n");
    }

    RunningServer server(file.DatabaseName());
    asio::io_context clientContext;
    auto client = Connect(clientContext, server.Port());
    REQUIRE(Request(client, "*2\r\n$3\r\nGET\r\n$7\r\npersist\r\n", 11) == "$5\r\nvalue\r\n");
}
