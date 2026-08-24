#include <chrono>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "RadishStore.h"

TEST_CASE("RadishStore creates, updates, renames, and scans live keys", "[store]")
{
    RadishStore<std::string> store(-1);

    REQUIRE(store.Create("first", "one") == -1);
    REQUIRE(store.Create("second", "two") == -1);
    REQUIRE(store.Create("first", "updated") == -1);
    REQUIRE(store.Rename("second", "renamed"));

    REQUIRE(store.Get("first") == "updated");
    REQUIRE(store.Get("renamed") == "two");
    REQUIRE_FALSE(store.Exists("second"));
    REQUIRE(store.Size() == 2);
}

TEST_CASE("RadishStore deletes, clears, and treats missing mutations as no-ops", "[store]")
{
    RadishStore<std::string> store(-1);
    store.Create("key", "value");

    REQUIRE(store.Delete("key"));
    REQUIRE_FALSE(store.Delete("key"));
    REQUIRE_FALSE(store.Rename("missing", "new-key"));
    REQUIRE_FALSE(store.Clear());

    store.Create("key", "value");
    REQUIRE(store.Clear());
    REQUIRE(store.Size() == 0);
    REQUIRE(store.Scan().empty());
}

TEST_CASE("RadishStore TTL expires entries and excludes them from live views", "[store][ttl]")
{
    RadishStore<std::string> store(50);
    store.Create("session", "user");

    REQUIRE(store.Exists("session"));
    std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });

    REQUIRE(store.IsExpired("session"));
    REQUIRE_FALSE(store.Exists("session"));
    REQUIRE_FALSE(store.Get("session").has_value());
    REQUIRE(store.Size() == 0);
    REQUIRE(store.Scan().empty());
}
