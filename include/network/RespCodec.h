#ifndef RADISH_RESP_CODEC_H
#define RADISH_RESP_CODEC_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace radish::network
{
enum class RespVersion
{
    Resp2,
    Resp3,
};

struct RespCommand
{
    std::vector<std::string> arguments;
};

struct RespParseResult
{
    enum class Status
    {
        Complete,
        Incomplete,
        Error,
    };

    Status status{ Status::Incomplete };
    RespCommand command{};
    std::size_t consumed{};
    std::string error{};
};

class RespCodec
{
public:
    static constexpr std::size_t kMaxArguments = 1024;
    static constexpr std::size_t kMaxBulkLength = 4 * 1024 * 1024;
    static constexpr std::size_t kMaxBufferedInput = 8 * 1024 * 1024;

    [[nodiscard]] static RespParseResult ParseCommand(std::string_view input);
    [[nodiscard]] static std::string EncodeCommand(const std::vector<std::string>& arguments);

    [[nodiscard]] static std::string SimpleString(std::string_view value);
    [[nodiscard]] static std::string Error(std::string_view message);
    [[nodiscard]] static std::string Integer(long long value);
    [[nodiscard]] static std::string BulkString(const std::optional<std::string>& value, RespVersion version);
    [[nodiscard]] static std::string Array(const std::vector<std::string>& values, RespVersion version);
    [[nodiscard]] static std::string Hello(RespVersion version);
};
}

#endif //RADISH_RESP_CODEC_H
