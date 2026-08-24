#ifndef RADISH_TCP_SERVER_H
#define RADISH_TCP_SERVER_H

#include <cstdint>
#include <memory>
#include <string>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "RadishDB.h"

namespace radish::network
{
class TcpServer
{
public:
    TcpServer(asio::io_context& context, RadishDB<std::string>& database, const std::string& bindAddress, std::uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t Port() const;

private:
    class Session;

    RadishDB<std::string>& m_database;
    asio::ip::tcp::acceptor m_acceptor;
    bool m_stopped{ false };

    void Accept();
};
}

#endif //RADISH_TCP_SERVER_H
