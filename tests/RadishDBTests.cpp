#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "RadishDB.h"
#include "TestSupport.h"

TEST_CASE("RadishDB persists inserts, updates, renames, and deletes across restart", "[database][e2e]")
{
    TemporaryDatabaseFile file{ "database-mutations" };

    {
        RadishDB<std::string> database(file.DatabaseName());
        database.Create("first", "one");
        database.Create("second", "two");
        database.Create("first", "updated");
        database.Rename("second", "renamed");
        database.Delete("first");

        REQUIRE(database.Size() == 1);
        REQUIRE_FALSE(database.Exists("first"));
        REQUIRE(database.Get("renamed") == "two");
    }

    RadishDB<std::string> database(file.DatabaseName());
    REQUIRE(database.Size() == 1);
    REQUIRE(database.Scan() == std::vector<std::string>{ "renamed" });
    REQUIRE(database.Get("renamed") == "two");
    REQUIRE_FALSE(database.Exists("first"));
}

TEST_CASE("RadishDB persists clear and ignores no-op mutations", "[database][e2e]")
{
    TemporaryDatabaseFile file{ "database-clear" };

    {
        RadishDB<std::string> database(file.DatabaseName());
        database.Create("one", "1");
        database.Create("two", "2");
        database.Clear();
        database.Delete("missing");
        database.Rename("missing", "still-missing");
    }

    RadishDB<std::string> database(file.DatabaseName());
    REQUIRE(database.Size() == 0);
    REQUIRE(database.Scan().empty());
    REQUIRE_FALSE(database.Exists("one"));
    REQUIRE_FALSE(database.Exists("still-missing"));
}

TEST_CASE("RadishDB preserves TTL expiry across restart", "[database][ttl][e2e]")
{
    TemporaryDatabaseFile file{ "database-ttl" };

    {
        RadishDB<std::string> database(file.DatabaseName(), 50);
        database.Create("session", "user");
        REQUIRE(database.Exists("session"));

        std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });

        REQUIRE(database.IsExpired("session"));
        REQUIRE_FALSE(database.Exists("session"));
        REQUIRE(database.Size() == 0);
    }

    RadishDB<std::string> database(file.DatabaseName(), 50);
    REQUIRE_FALSE(database.Exists("session"));
    REQUIRE(database.Size() == 0);
}

TEST_CASE("RadishDB compaction retains the live materialized state", "[database][compaction][e2e]")
{
    TemporaryDatabaseFile file{ "database-compaction" };

    {
        RadishDB<std::string> database(file.DatabaseName());
        database.Create("discarded", "old");
        database.Create("kept", "initial");
        database.Create("kept", "latest");
        database.Rename("kept", "final");
        database.Delete("discarded");
        database.Compact();
    }

    RadishDB<std::string> database(file.DatabaseName());
    REQUIRE(database.Size() == 1);
    REQUIRE(database.Get("final") == "latest");
    REQUIRE_FALSE(database.Exists("kept"));
    REQUIRE_FALSE(database.Exists("discarded"));
}

TEST_CASE("RadishDB volatile mode does not create or replay an AOF", "[database][volatile]")
{
    TemporaryDatabaseFile file{ "database-volatile" };

    {
        RadishDB<std::string> database(file.DatabaseName(), PersistenceMode::Disabled);
        database.Create("ephemeral", "value");
        REQUIRE(database.Get("ephemeral") == "value");
        REQUIRE_FALSE(std::filesystem::exists(file.LogFile()));
    }

    RadishDB<std::string> restarted(file.DatabaseName(), PersistenceMode::Disabled);
    REQUIRE_FALSE(restarted.Exists("ephemeral"));
}
