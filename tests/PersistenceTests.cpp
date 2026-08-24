#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "RadishDB.h"
#include "persistence/LogReader.h"
#include "persistence/LogWriter.h"

namespace {
std::filesystem::path TestFile(const std::string& name) {
    const auto file = std::filesystem::temp_directory_path() / ("radish-" + name + ".radish");
    std::filesystem::remove(file);
    std::filesystem::remove(file.string() + ".tmp");
    return file;
}

void ReplayAppliesEveryOperation() {
    const auto file = TestFile("replay-all-events");
    LogWriter<std::string> writer(file.filename().string(), file.parent_path().string());
    writer.Append(RadishEvent<std::string>{ CREATE, -1, "old", std::nullopt, "value" });
    writer.Append(RadishEvent<std::string>{ RENAME, -1, "old", "new" });
    writer.Append(RadishEvent<std::string>{ DELETE, -1, "new" });
    writer.Append(RadishEvent<std::string>{ CREATE, -1, "discarded", std::nullopt, "value" });
    writer.Append(RadishEvent<std::string>{ CLEAR, -1 });
    writer.Append(RadishEvent<std::string>{ CREATE, -1, "survives", std::nullopt, "final" });

    RadishStore<std::string> store(-1);
    writer.Replay(store);
    assert(store.Size() == 1);
    assert(store.Get("survives").value() == "final");

    writer.Compact();
    RadishStore<std::string> compactedStore(-1);
    writer.Replay(compactedStore);
    assert(compactedStore.Size() == 1);
    assert(compactedStore.Get("survives").value() == "final");

    std::filesystem::remove(file);
}

void CompactionKeepsRenamedKey() {
    const auto file = TestFile("compaction-rename");
    LogWriter<std::string> writer(file.filename().string(), file.parent_path().string());
    writer.Append(RadishEvent<std::string>{ CREATE, -1, "before", std::nullopt, "value" });
    writer.Append(RadishEvent<std::string>{ RENAME, -1, "before", "after" });
    writer.Compact();

    RadishStore<std::string> store(-1);
    writer.Replay(store);
    assert(!store.Exists("before"));
    assert(store.Get("after").value() == "value");

    std::filesystem::remove(file);
}

void DatabaseRestoresUncompactedLog() {
    const auto file = TestFile("database-restart");
    const auto databaseName = file.string() + "-database";
    std::filesystem::remove(databaseName + ".radish");

    {
        RadishDB<std::string> database(databaseName);
        database.Create("old", "value");
        database.Rename("old", "new");
        database.Delete("new");
        database.Create("survives", "final");
    }

    {
        RadishDB<std::string> database(databaseName);
        assert(database.Size() == 1);
        assert(!database.Exists("old"));
        assert(database.Get("survives").value() == "final");
    }

    std::filesystem::remove(databaseName + ".radish");
}

void IgnoresIncompleteFinalRecord() {
    const auto file = TestFile("truncated-record");
    LogWriter<std::string> writer(file.filename().string(), file.parent_path().string());
    writer.Append(RadishEvent<std::string>{ CREATE, -1, "key", std::nullopt, "value" });
    std::filesystem::resize_file(file, std::filesystem::file_size(file) - 1);

    LogReader<std::string> reader(file.string());
    assert(reader.GetEvents().empty());

    std::filesystem::remove(file);
}
}

int main() {
    ReplayAppliesEveryOperation();
    CompactionKeepsRenamedKey();
    DatabaseRestoresUncompactedLog();
    IgnoresIncompleteFinalRecord();
}
