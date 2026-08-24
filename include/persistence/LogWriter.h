//
// Created by Dorza on 5/22/2026.
//

#ifndef RADISH_LOGWRITER_H
#define RADISH_LOGWRITER_H


#include <fstream>
#include <stdexcept>
#include <memory>
#include <array>
#include <filesystem>
#include <sstream>

#include "CompactStrategy.h"
#include "LogReader.h"
#include "../RadishStore.h"
#include "../enums/EventType.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class LogWriter
{
public:
    using Events          = PersistenceLogTypes<TValue>::Events;
    using Event           = PersistenceLogTypes<TValue>::Event;
    using EventsMap       = PersistenceLogTypes<TValue>::EventsMap;
    using EventStrategies = PersistenceLogTypes<TValue>::EventStrategies;

    LogWriter(const std::string& filename, const std::string& path)
        : m_filename{ (std::filesystem::path(path) / filename).string() }
        , m_reader{ m_filename }
    {
        EnsureInitialized();
        InitializeCompactStrategies();
    }

    void Append(const Event& row)
    {
        std::ofstream file(m_filename, std::ios::binary | std::ios::app);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + m_filename);
        }

        WriteRecord(file, row);
        file.flush();

        if (!file) {
            throw std::ios_base::failure("Failed to flush persistence log: " + m_filename);
        }
    }

    void Replay(RadishStore<TValue>& store)
    {
        for (const auto& event : m_reader.GetEvents()) {
            static_cast<void>(ApplyEvent(store, event));
        }
    }

    [[nodiscard]]
    bool ApplyEvent(RadishStore<TValue>& store, const Event& event) const
    {
        switch (event.GetEventType()) {
            case CREATE:
                store.Create(*event.GetKey(), *event.GetPayload(), event.GetTimestamp());
                return true;
            case RENAME:
                return store.Rename(*event.GetKey(), *event.GetRenameKey());
            case DELETE:
                return store.Delete(*event.GetKey());
            case CLEAR:
                return store.Clear();
            default:
                break;
        }

        throw std::runtime_error("Unknown persistence event type");
    }

    void Compact()
    {
        EventsMap events;
        const SystemClock clock{};

        for (auto& event : m_reader.GetEvents()) {
            if (event.GetEventType() == CREATE
                && event.GetTimestamp() != -1
                && event.GetTimestamp() < clock.Now()) {
                continue;
            }

            if (m_compactStrategies.contains(event.GetEventType())) {
                m_compactStrategies[event.GetEventType()]->Execute(events, event);
            }
        }

        RewriteHistory(events);
    }

private:
    std::string m_filename;
    LogReader<TValue> m_reader;
    EventStrategies m_compactStrategies;

    void RewriteHistory(const EventsMap& events)
    {
        const SystemClock clock{};
        const auto temporaryFilename = m_filename + ".tmp";
        std::ofstream file(temporaryFilename, std::ios::binary | std::ios::trunc);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open temporary persistence log: " + temporaryFilename);
        }

        WriteHeader(file);

        for (const auto& [_, event] : events) {
            if (event.GetTimestamp() != -1 && event.GetTimestamp() < clock.Now()) {
                continue;
            }

            WriteRecord(file, event);
        }

        file.flush();
        if (!file) {
            throw std::ios_base::failure("Failed to flush compacted persistence log");
        }

        file.close();
        if (file.fail()) {
            throw std::ios_base::failure("Failed to close compacted persistence log");
        }

        std::filesystem::rename(temporaryFilename, m_filename);
    }

    void InitializeCompactStrategies()
    {
        m_compactStrategies.emplace(CREATE, std::make_unique<CreateCompactStrategy<TValue>>());
        m_compactStrategies.emplace(RENAME, std::make_unique<RenameCompactStrategy<TValue>>());
        m_compactStrategies.emplace(DELETE, std::make_unique<DeleteCompactStrategy<TValue>>());
        m_compactStrategies.emplace(CLEAR,  std::make_unique<ClearCompactStrategy<TValue>>());
    }

    void EnsureInitialized() const
    {
        if (std::filesystem::exists(m_filename)) {
            std::ifstream file(m_filename, std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open persistence log: " + m_filename);
            }
            std::array<char, 8> header{};
            file.read(header.data(), static_cast<std::streamsize>(header.size()));
            constexpr std::array<char, 8> expectedHeader{ 'R', 'A', 'D', 'I', 'S', 'H', '\0', '\1' };
            if (!file || header != expectedHeader) {
                throw std::runtime_error("Unsupported or corrupt persistence log header");
            }
            return;
        }

        std::ofstream file(m_filename, std::ios::binary | std::ios::trunc);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to create persistence log: " + m_filename);
        }

        WriteHeader(file);
        file.flush();

        if (!file) {
            throw std::ios_base::failure("Failed to initialize persistence log: " + m_filename);
        }
    }

    static void WriteHeader(std::ostream& file)
    {
        constexpr std::array<char, 8> header{ 'R', 'A', 'D', 'I', 'S', 'H', '\0', '\1' };

        file.write(header.data(), static_cast<std::streamsize>(header.size()));

        if (!file) {
            throw std::ios_base::failure("Failed to write persistence log header");
        }
    }

    static void WriteRecord(std::ostream& file, const Event& event)
    {
        std::ostringstream record(std::ios::binary | std::ios::out);
        event.Serialize(record);
        const auto bytes = record.str();

        if (bytes.empty() || bytes.size() > 64 * 1024 * 1024) {
            throw std::length_error("Persistence record has invalid size");
        }

        const auto size = static_cast<BinarySize>(bytes.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

        if (!file) {
            throw std::ios_base::failure("Failed to write persistence record");
        }
    }
};


#endif //RADISH_LOGWRITER_H