#include "network/RespCodec.h"

#include <charconv>

namespace radish::network
{
namespace
{
enum class NumberStatus
{
    Complete,
    Incomplete,
    Error,
};

NumberStatus ParseNumber(const std::string_view input, const std::size_t offset, long long& value, std::size_t& next)
{
    const auto terminator = input.find("\r\n", offset);

    if (terminator == std::string_view::npos) {
        return NumberStatus::Incomplete;
    }

    if (terminator == offset) {
        return NumberStatus::Error;
    }

    const auto number = input.substr(offset, terminator - offset);
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);

    if (error != std::errc{} || end != number.data() + number.size()) {
        return NumberStatus::Error;
    }

    next = terminator + 2;

    return NumberStatus::Complete;
}

std::string SanitizeSimpleValue(const std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (const auto character : value) {
        result += (character == '\r' || character == '\n') ? ' ' : character;
    }

    return result;
}
}

RespParseResult RespCodec::ParseCommand(const std::string_view input)
{
    if (input.empty()) {
        return {};
    }

    if (input.front() != '*') {
        return { RespParseResult::Status::Error, {}, 0, "Protocol error: expected an array request" };
    }

    long long count{};
    std::size_t offset{};
    const auto countStatus = ParseNumber(input, 1, count, offset);

    if (countStatus == NumberStatus::Incomplete) {
        return {};
    }

    if (countStatus == NumberStatus::Error || count <= 0 || count > static_cast<long long>(kMaxArguments)) {
        return { RespParseResult::Status::Error, {}, 0, "Protocol error: invalid argument count" };
    }

    RespCommand command;
    command.arguments.reserve(count);

    for (long long index = 0; index < count; ++index) {
        if (offset == input.size()) {
            return {};
        }

        if (input[offset] != '$') {
            return { RespParseResult::Status::Error, {}, 0, "Protocol error: expected a bulk string" };
        }

        long long length{};
        std::size_t dataOffset{};
        const auto lengthStatus = ParseNumber(input, offset + 1, length, dataOffset);

        if (lengthStatus == NumberStatus::Incomplete) {
            return {};
        }

        if (lengthStatus == NumberStatus::Error || length < 0 || length > static_cast<long long>(kMaxBulkLength)) {
            return { RespParseResult::Status::Error, {}, 0, "Protocol error: invalid bulk string length" };
        }

        const auto bulkLength = static_cast<std::size_t>(length);

        if (dataOffset > input.size() || bulkLength > input.size() - dataOffset) {
            return {};
        }

        if (input.size() - dataOffset - bulkLength < 2) {
            return {};
        }

        if (input[dataOffset + bulkLength] != '\r' || input[dataOffset + bulkLength + 1] != '\n') {
            return { RespParseResult::Status::Error, {}, 0, "Protocol error: invalid bulk string terminator" };
        }

        command.arguments.emplace_back(input.substr(dataOffset, bulkLength));
        offset = dataOffset + bulkLength + 2;
    }

    return {
        RespParseResult::Status::Complete,
        std::move(command),
        offset,
        {}
    };
}

std::string RespCodec::EncodeCommand(const std::vector<std::string>& arguments)
{
    std::string result = "*" + std::to_string(arguments.size()) + "\r\n";

    for (const auto& argument : arguments) {
        result += "$" + std::to_string(argument.size()) + "\r\n" + argument + "\r\n";
    }

    return result;
}

std::string RespCodec::SimpleString(const std::string_view value)
{
    return "+" + SanitizeSimpleValue(value) + "\r\n";
}

std::string RespCodec::Error(const std::string_view message)
{
    return "-ERR " + SanitizeSimpleValue(message) + "\r\n";
}

std::string RespCodec::Integer(const long long value)
{
    return ":" + std::to_string(value) + "\r\n";
}

std::string RespCodec::BulkString(const std::optional<std::string>& value, const RespVersion version)
{
    if (!value) {
        return version == RespVersion::Resp3 ? "_\r\n" : "$-1\r\n";
    }

    return "$" + std::to_string(value->size()) + "\r\n" + *value + "\r\n";
}

std::string RespCodec::Array(const std::vector<std::string>& values, const RespVersion)
{
    std::string result = "*" + std::to_string(values.size()) + "\r\n";
    for (const auto& value : values) {
        result += BulkString(value, RespVersion::Resp2);
    }
    return result;
}

std::string RespCodec::Hello(const RespVersion version)
{
    const auto protocol = version == RespVersion::Resp3
        ? "3"
        : "2";

    const std::vector<std::string> values{
        "server", "radish",
        "version", "0.1",
        "proto", protocol,
        "mode", "standalone",
        "role", "master",
        "modules", "",
    };

    if (version == RespVersion::Resp2) {
        return Array(values, version);
    }

    std::string result = "%6\r\n";

    for (std::size_t index = 0; index < values.size() - 2; index += 2) {
        result += BulkString(values[index], version);
        result += BulkString(values[index + 1], version);
    }

    result += BulkString("modules", version);
    result += "*0\r\n";

    return result;
}
}
