//
// Created by Dorza on 5/22/2026.
//

#ifndef RADISH_LOGPARSER_H
#define RADISH_LOGPARSER_H


#include "CompactStrategy.h"
#include "LogReader.h"
#include "../RadishStore.h"
#include "../enums/EventType.h"


template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class LogWriter {
public:
    using Events          = PersistenceLogTypes<TValue>::Events;
    using Event           = PersistenceLogTypes<TValue>::Event;
    using EventsMap       = PersistenceLogTypes<TValue>::EventsMap;
    using EventStrategies = PersistenceLogTypes<TValue>::EventStrategies;

    LogWriter(const std::string& filename, const std::string& path)
        : m_reader{ filename, path }
        , m_filename{ filename }
        , m_path{ path }
    {
        InitializeCompactStrategies();
    }

    ~LogWriter() {
        try {
            std::cout << "Compacting log file before destruction...\n";
            Compact();
        }
        catch (...) {
            std::cout << "Exception caught in ~LogWriter()\n";
        }
    }

    void Append(const Event& row) {
        std::ofstream file(m_filename, std::ios::binary | std::ios::app);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + m_filename);
        }

        row.Serialize(file);
    }

    void Replay(RadishStore<TValue>& store) {
        for (auto& event : m_reader.GetEvents()) {
            if (IsReplayableEvent(event) == false) {
                continue;
            }

            store.Create(*event.GetKey(), *event.GetPayload(), event.GetTimestamp());
        }
    }

    void Compact() {
        EventsMap events;

        for (auto& event : m_reader.GetEvents()) {
            if (m_compactStrategies.contains(event.GetEventType())) {

                m_compactStrategies[event.GetEventType()]->Execute(events, event);
            }
        }

        RewriteHistory(events);
    }

private:
    LogReader<TValue> m_reader;
    EventStrategies m_compactStrategies;
    std::string m_filename;
    std::string m_path;

    bool IsReplayableEvent(const RadishEvent<TValue>& event) const {
        if (event.GetEventType() != CREATE || event.GetKey() == std::nullopt || event.GetPayload() == std::nullopt) {
            return false;
        }

        return true;
    }

    void RewriteHistory(const EventsMap& events) {
        const SystemClock clock{};
        std::ofstream file((m_filename.data()), std::ios::binary | std::ios::trunc);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for rewriting: " + std::string(m_filename));
        }

        for (const auto& [_, event] : events) {
            if (event.GetTimestamp() != -1 && event.GetTimestamp() < clock.Now()) {
                continue;
            }

            event.Serialize(file);
        }
    }

    void InitializeCompactStrategies() {
        m_compactStrategies.emplace(CREATE, std::make_unique<CreateCompactStrategy<TValue>>());
        m_compactStrategies.emplace(RENAME, std::make_unique<RenameCompactStrategy<TValue>>());
        m_compactStrategies.emplace(DELETE, std::make_unique<DeleteCompactStrategy<TValue>>());
        m_compactStrategies.emplace(CLEAR,  std::make_unique<ClearCompactStrategy<TValue>>());
    }
};


#endif //RADISH_LOGPARSER_H