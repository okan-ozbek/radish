#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "network/RespCodec.h"

using radish::network::RespCodec;
using radish::network::RespParseResult;
using radish::network::RespVersion;

TEST_CASE("RESP codec parses fragmented and pipelined bulk-string commands", "[resp][codec]")
{
    const std::string request = "*2\r\n$4\r\nPING\r\n$5\r\nhello\r\n";
    REQUIRE(RespCodec::ParseCommand(request.substr(0, 12)).status == RespParseResult::Status::Incomplete);

    const auto parsed = RespCodec::ParseCommand(request + "*1\r\n$4\r\nPING\r\n");
    REQUIRE(parsed.status == RespParseResult::Status::Complete);
    REQUIRE(parsed.command.arguments == std::vector<std::string>{ "PING", "hello" });
    REQUIRE(parsed.consumed == request.size());

    const auto second = RespCodec::ParseCommand((request + "*1\r\n$4\r\nPING\r\n").substr(parsed.consumed));
    REQUIRE(second.status == RespParseResult::Status::Complete);
    REQUIRE(second.command.arguments == std::vector<std::string>{ "PING" });
}

TEST_CASE("RESP codec preserves binary-safe bulk strings", "[resp][codec]")
{
    const std::string request{ "*2\r\n$3\r\nSET\r\n$3\r\n", 17 };
    const std::string binaryRequest = request + std::string{ "a\0b", 3 } + "\r\n";
    const auto parsed = RespCodec::ParseCommand(binaryRequest);

    REQUIRE(parsed.status == RespParseResult::Status::Complete);
    REQUIRE(parsed.command.arguments[1] == std::string{ "a\0b", 3 });
}

TEST_CASE("RESP codec rejects malformed and oversized frames", "[resp][codec][failure]")
{
    REQUIRE(RespCodec::ParseCommand("+PING\r\n").status == RespParseResult::Status::Error);
    REQUIRE(RespCodec::ParseCommand("*1\r\n+PING\r\n").status == RespParseResult::Status::Error);
    REQUIRE(RespCodec::ParseCommand("*1025\r\n").status == RespParseResult::Status::Error);
    REQUIRE(RespCodec::ParseCommand("*1\r\n$4194305\r\n").status == RespParseResult::Status::Error);
}

TEST_CASE("RESP codec emits RESP2 and RESP3 compatible replies", "[resp][codec]")
{
    REQUIRE(RespCodec::BulkString(std::nullopt, RespVersion::Resp2) == "$-1\r\n");
    REQUIRE(RespCodec::BulkString(std::nullopt, RespVersion::Resp3) == "_\r\n");
    REQUIRE(RespCodec::Hello(RespVersion::Resp2).starts_with("*12\r\n"));
    REQUIRE(RespCodec::Hello(RespVersion::Resp3).starts_with("%6\r\n"));
}
