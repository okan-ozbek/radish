#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "RadishEvent.h"

TEST_CASE("RadishEvent round-trips create payloads through binary serialization", "[event][binary]")
{
    const RadishEvent<std::vector<std::byte>> event{
        CREATE,
        123,
        "blob",
        std::nullopt,
        std::vector<std::byte>{ std::byte{ 0x00 }, std::byte{ 0xFF } }
    };
    std::ostringstream output(std::ios::binary | std::ios::out);
    event.Serialize(output);

    RadishEvent<std::vector<std::byte>> restored;
    std::istringstream input(output.str(), std::ios::binary | std::ios::in);
    restored.Deserialize(input);

    REQUIRE(restored.GetEventType() == CREATE);
    REQUIRE(restored.GetTimestamp() == 123);
    REQUIRE(restored.GetKey() == "blob");
    REQUIRE(restored.GetPayload() == event.GetPayload());
}

TEST_CASE("RadishEvent round-trips rename, delete, and clear fields", "[event][binary]")
{
    const RadishEvent<std::string> rename{ RENAME, -1, "old", "new" };
    const RadishEvent<std::string> deletion{ DELETE, -1, "key" };
    const RadishEvent<std::string> clear{ CLEAR, -1 };

    for (const auto& event : { rename, deletion, clear }) {
        std::ostringstream output(std::ios::binary | std::ios::out);
        event.Serialize(output);

        RadishEvent<std::string> restored;
        std::istringstream input(output.str(), std::ios::binary | std::ios::in);
        restored.Deserialize(input);

        REQUIRE(restored.GetEventType() == event.GetEventType());
        REQUIRE(restored.GetTimestamp() == event.GetTimestamp());
        REQUIRE(restored.GetKey() == event.GetKey());
        REQUIRE(restored.GetRenameKey() == event.GetRenameKey());
        REQUIRE_FALSE(restored.GetPayload().has_value());
    }
}

TEST_CASE("RadishEvent rejects unsupported event types", "[event][failure]")
{
    const RadishEvent<int> event{ static_cast<EventType>(255), -1 };
    std::ostringstream output(std::ios::binary | std::ios::out);

    REQUIRE_THROWS(event.Serialize(output));
}
