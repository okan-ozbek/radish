#include "network/TcpServer.h"

#include <array>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <asio/bind_executor.hpp>
#include <asio/write.hpp>

#include "network/RespCodec.h"

namespace radish::network
{
namespace
{
std::string Uppercase(std::string value)
{
    for (auto& character : value) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    return value;
}
}

class TcpServer::Session final : public std::enable_shared_from_this<TcpServer::Session>
{
public:
    Session(asio::ip::tcp::socket socket, RadishDB<std::string>& database)
        : m_socket{ std::move(socket) }
        , m_database{ database }
    {}

    void Start()
    {
        Read();
    }

private:
    asio::ip::tcp::socket m_socket;
    RadishDB<std::string>& m_database;
    std::array<char, 8192> m_readBuffer{};
    std::string m_input;
    std::deque<std::string> m_output;
    RespVersion m_version{ RespVersion::Resp2 };
    bool m_closeAfterWrite{ false };

    void Read()
    {
        const auto self = shared_from_this();
        m_socket.async_read_some(asio::buffer(m_readBuffer), [self](const asio::error_code& error, const std::size_t bytesRead) {
            if (error) {
                return;
            }

            self->m_input.append(self->m_readBuffer.data(), bytesRead);

            if (self->m_input.size() > RespCodec::kMaxBufferedInput) {
                self->Queue(RespCodec::Error("Protocol error: input buffer limit exceeded"), true);
                return;
            }

            self->Process();

            if (!self->m_closeAfterWrite) {
                self->Read();
            }
        });
    }

    void Process()
    {
        while (!m_input.empty() && !m_closeAfterWrite) {
            auto [status, command, consumed, error] = RespCodec::ParseCommand(m_input);
            if (status == RespParseResult::Status::Incomplete) {
                return;
            }

            if (status == RespParseResult::Status::Error) {
                Queue(RespCodec::Error(error), true);
                return;
            }

            m_input.erase(0, consumed);
            auto [response, close] = Dispatch(command);
            Queue(std::move(response), close);
        }
    }

    using CommandResult = std::pair<std::string, bool>;
    using CommandHandler = CommandResult (Session::*)(const RespCommand&, std::string_view);

    CommandResult HandleHello(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() == 1 || command.arguments.size() > 2) {
            return WrongArity(name);
        }

        if (command.arguments[1] == "2") {
            m_version = RespVersion::Resp2;
        }
        else if (command.arguments[1] == "3") {
            m_version = RespVersion::Resp3;
        }
        else {
            return {
                RespCodec::Error("NOPROTO unsupported protocol version"),
                false
            };
        }
        return {
            RespCodec::Hello(m_version),
            false
        };
    }

    CommandResult HandlePing(const RespCommand& command, const std::string_view name) {
        if (command.arguments.size() == 1) {
            return {
                RespCodec::SimpleString("PONG"),
                false
            };
        }

        if (command.arguments.size() == 2) {
            return { RespCodec::BulkString(command.arguments[1], m_version), false };
        }

        return WrongArity(name);
    }

    CommandResult HandleGet(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() != 2) {
            return WrongArity(name);
        }

        return {
            RespCodec::BulkString(m_database.Get(command.arguments[1]), m_version),
            false
        };
    }

    CommandResult HandleSet(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() != 3) {
            return WrongArity(name);
        }

        m_database.Create(command.arguments[1], command.arguments[2]);

        return {
            RespCodec::SimpleString("OK"),
            false
        };
    }

    CommandResult HandleDelete(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() < 2) {
            return WrongArity(name);
        }

        long long deleted{};

        for (std::size_t index = 1; index < command.arguments.size(); ++index) {
            deleted += m_database.Delete(command.arguments[index]) ? 1 : 0;
        }

        return {
            RespCodec::Integer(deleted),
            false
        };
    }

    CommandResult HandleRename(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() != 3) {
            return WrongArity(name);
        }

        if (!m_database.Rename(command.arguments[1], command.arguments[2])) {
            return {
                RespCodec::Error("no such key"),
                false
            };
        }

        return {
            RespCodec::SimpleString("OK"),
            false
        };
    }

    CommandResult HandleExists(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() < 2) {
            return WrongArity(name);
        }

        long long exists{};

        for (std::size_t index = 1; index < command.arguments.size(); ++index) {
            exists += m_database.Exists(command.arguments[index]) ? 1 : 0;
        }

        return {
            RespCodec::Integer(exists),
            false
        };
    }

    CommandResult HandleKeys(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() != 2) {
            return WrongArity(name);
        }

        if (command.arguments[1] != "*") {
            return {
                RespCodec::Error("only KEYS * is supported"),
                false
            };
        }

        return {
            RespCodec::Array(m_database.Scan(), m_version),
            false
        };
    }

    CommandResult HandleDatabaseSize(const RespCommand& command, const std::string_view name)
    {
        if (command.arguments.size() != 1) {
            return WrongArity(name);
        }

        return {
            RespCodec::Integer(static_cast<long long>(m_database.Size())),
            false
        };
    }

    CommandResult HandleFlushDatabase(const RespCommand& command, const std::string_view name) {
        if (command.arguments.size() != 1) {
            return WrongArity(name);
        }
        m_database.Clear();
        return {
            RespCodec::SimpleString("OK"),
            false
        };
    }

    CommandResult HandleQuit(const RespCommand& command, std::string_view name) {
        if (command.arguments.size() != 1) {
            return WrongArity(name);
        }
        return {
            RespCodec::SimpleString("OK"),
            true
        };
    }

    static const std::unordered_map<std::string, CommandHandler>& CommandHandlers() {
        static const std::unordered_map<std::string, CommandHandler> handlers{
            { "HELLO", &Session::HandleHello },
            { "PING", &Session::HandlePing },
            { "GET", &Session::HandleGet },
            { "SET", &Session::HandleSet },
            { "DEL", &Session::HandleDelete },
            { "RENAME", &Session::HandleRename },
            { "EXISTS", &Session::HandleExists },
            { "KEYS", &Session::HandleKeys },
            { "DBSIZE", &Session::HandleDatabaseSize },
            { "FLUSHDB", &Session::HandleFlushDatabase },
            { "QUIT", &Session::HandleQuit },
        };
        return handlers;
    }

    static CommandResult WrongArity(const std::string_view name)
    {
        return {
            RespCodec::Error("wrong number of arguments for '" + std::string(name) + "' command"),
            false
        };
    }

    CommandResult Dispatch(const RespCommand& command)
    {
        const auto name = Uppercase(command.arguments.front());
        const auto handler = CommandHandlers().find(name);

        if (handler == CommandHandlers().end()) {
            return { RespCodec::Error("unknown command '" + name + "'"), false };
        }

        return (this->*(handler->second))(command, name);
    }

    void Queue(std::string response, const bool closeAfterWrite)
    {
        const auto writing = !m_output.empty();

        m_output.push_back(std::move(response));
        m_closeAfterWrite = m_closeAfterWrite || closeAfterWrite;

        if (!writing) {
            Write();
        }
    }

    void Write()
    {
        const auto self = shared_from_this();

        asio::async_write(m_socket, asio::buffer(m_output.front()), [self](const asio::error_code& error, const std::size_t) {
            if (error) {
                return;
            }

            self->m_output.pop_front();

            if (!self->m_output.empty()) {
                self->Write();
            }
            else if (self->m_closeAfterWrite) {
                asio::error_code ignored;
                const auto shutdownResult = self->m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);

                if (const auto closeResult = self->m_socket.close(ignored); shutdownResult || closeResult) {
                    return;
                }
            }
        });
    }
};

TcpServer::TcpServer(
    asio::io_context& context,
    RadishDB<std::string>& database,
    const std::string& bindAddress,
    const std::uint16_t port
)
    : m_database{ database }
    , m_acceptor{ context }
{
    const auto address = asio::ip::make_address(bindAddress);
    const asio::ip::tcp::endpoint endpoint{ address, port };
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
}

TcpServer::~TcpServer()
{
    Stop();
}

void TcpServer::Start()
{
    Accept();
}

void TcpServer::Stop()
{
    if (m_stopped) {
        return;
    }

    m_stopped = true;

    asio::error_code ignored;
    const auto cancelResult = m_acceptor.cancel(ignored);

    if (const auto closeResult = m_acceptor.close(ignored); cancelResult || closeResult) {
        return;
    }
}

std::uint16_t TcpServer::Port() const
{
    return m_acceptor.local_endpoint().port();
}

void TcpServer::Accept()
{
    if (m_stopped) {
        return;
    }

    m_acceptor.async_accept([this](const asio::error_code& error, asio::ip::tcp::socket socket) {
        if (!error) {
            std::make_shared<Session>(std::move(socket), m_database)->Start();
        }

        if (!m_stopped) {
            Accept();
        }
    });
}
}
