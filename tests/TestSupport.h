#ifndef RADISH_TEST_SUPPORT_H
#define RADISH_TEST_SUPPORT_H

#include <filesystem>
#include <string>

class TemporaryDatabaseFile {
public:
    explicit TemporaryDatabaseFile(const std::string& name)
        : m_databaseName{ (std::filesystem::temp_directory_path() / ("radish-catch2-" + name)).string() }
    {
        Remove();
    }

    ~TemporaryDatabaseFile()
    {
        Remove();
    }

    [[nodiscard]]
    const std::string& DatabaseName() const
    {
        return m_databaseName;
    }

    [[nodiscard]]
    std::filesystem::path LogFile() const
    {
        return m_databaseName + ".radish";
    }

    [[nodiscard]]
    std::string LogFilename() const
    {
        return LogFile().filename().string();
    }

    [[nodiscard]]
    std::string LogDirectory() const
    {
        return LogFile().parent_path().string();
    }

private:
    std::string m_databaseName;

    void Remove() const
    {
        std::filesystem::remove(LogFile());
        std::filesystem::remove(LogFile().string() + ".tmp");
    }
};

#endif //RADISH_TEST_SUPPORT_H
