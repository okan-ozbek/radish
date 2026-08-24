#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "RadishStore.h"
#include "persistence/LogReader.h"
#include "persistence/PersistenceLog.h"
#include "TestSupport.h"

TEST_CASE("PersistenceLog replays all supported mutations", "[persistence][read][write]")
{
    TemporaryDatabaseFile file{ "persistence-replay" };
    PersistenceLog<std::string> log(file.LogFilename(), file.LogDirectory());
    log.Append(RadishEvent<std::string>{ CREATE, -1, "old", std::nullopt, "value" });
    log.Append(RadishEvent<std::string>{ RENAME, -1, "old", "new" });
    log.Append(RadishEvent<std::string>{ DELETE, -1, "new" });
    log.Append(RadishEvent<std::string>{ CREATE, -1, "discarded", std::nullopt, "value" });
    log.Append(RadishEvent<std::string>{ CLEAR, -1 });
    log.Append(RadishEvent<std::string>{ CREATE, -1, "survives", std::nullopt, "final" });

    RadishStore<std::string> store(-1);
    log.Replay(store);

    REQUIRE(store.Size() == 1);
    REQUIRE(store.Get("survives") == "final");
}

TEST_CASE("PersistenceLog writes and restores raw byte payloads", "[persistence][binary]")
{
    TemporaryDatabaseFile file{ "persistence-bytes" };
    const std::vector<std::byte> payload{ std::byte{ 0x00 }, std::byte{ 0x01 }, std::byte{ 0xFE }, std::byte{ 0xFF } };
    PersistenceLog<std::vector<std::byte>> log(file.LogFilename(), file.LogDirectory());
    log.Append(RadishEvent<std::vector<std::byte>>{ CREATE, -1, "blob", std::nullopt, payload });

    std::ifstream binary(file.LogFile(), std::ios::binary);
    REQUIRE(binary.is_open());
    std::array<char, 8> header{};
    const std::array<char, 8> expectedHeader{ 'R', 'A', 'D', 'I', 'S', 'H', '\0', '\1' };
    binary.read(header.data(), static_cast<std::streamsize>(header.size()));
    REQUIRE(header == expectedHeader);

    RadishStore<std::vector<std::byte>> store(-1);
    log.Replay(store);
    REQUIRE(store.Get("blob") == payload);
}

TEST_CASE("PersistenceLog compaction preserves replayed state", "[persistence][compaction]")
{
    TemporaryDatabaseFile file{ "persistence-compaction" };
    PersistenceLog<std::string> log(file.LogFilename(), file.LogDirectory());
    log.Append(RadishEvent<std::string>{ CREATE, -1, "before", std::nullopt, "value" });
    log.Append(RadishEvent<std::string>{ RENAME, -1, "before", "after" });
    log.Compact();

    RadishStore<std::string> store(-1);
    log.Replay(store);
    REQUIRE_FALSE(store.Exists("before"));
    REQUIRE(store.Get("after") == "value");
}

TEST_CASE("PersistenceLog rejects an invalid header and inaccessible write path", "[persistence][failure]")
{
    TemporaryDatabaseFile file{ "persistence-invalid-header" };
    {
        std::ofstream binary(file.LogFile(), std::ios::binary);
        binary.write("invalid!", 8);
    }

    LogReader<std::string> reader(file.LogFile().string());
    REQUIRE_THROWS(reader.GetEvents());

    const auto missingDirectory = std::filesystem::temp_directory_path() / "radish-catch2-missing-directory";
    std::filesystem::remove_all(missingDirectory);
    REQUIRE_THROWS((PersistenceLog<std::string>{ "data.radish", missingDirectory.string() }));
}

TEST_CASE("PersistenceLog ignores a torn final write after sudden shutdown", "[persistence][recovery]")
{
    TemporaryDatabaseFile file{ "persistence-torn-record" };
    {
        PersistenceLog<std::string> log(file.LogFilename(), file.LogDirectory());
        log.Append(RadishEvent<std::string>{ CREATE, -1, "key", std::nullopt, "value" });
    }

    std::filesystem::resize_file(file.LogFile(), std::filesystem::file_size(file.LogFile()) - 1);

    LogReader<std::string> reader(file.LogFile().string());
    REQUIRE(reader.GetEvents().empty());
}
