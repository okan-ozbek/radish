//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHFILE_H
#define RADISH_RADISHFILE_H


#include <fstream>
#include <string>
#include <utility>
#include <stdexcept>

#include "CompactStrategy.h"
#include "../RadishEvent.h"

template<typename TValue> class RadishStore;

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class PersistenceLog {
public:
    using Events          = PersistenceLogTypes<TValue>::Events;
    using Event           = PersistenceLogTypes<TValue>::Event;
    using EventsMap       = PersistenceLogTypes<TValue>::EventsMap;
    using EventStrategies = PersistenceLogTypes<TValue>::EventStrategies;

    PersistenceLog() = delete;

    PersistenceLog(std::string filename, std::string path)
        : m_filename{ std::move(filename) }
        , m_path{ std::move(path) }
    {
        m_compactStrategies[CREATE] = std::make_unique<CreateCompactStrategy<TValue>>();
        m_compactStrategies[RENAME] = std::make_unique<RenameCompactStrategy<TValue>>();
        m_compactStrategies[DELETE] = std::make_unique<DeleteCompactStrategy<TValue>>();
        m_compactStrategies[CLEAR]  = std::make_unique<ClearCompactStrategy<TValue>>();
    }

    ~PersistenceLog() {
        Compact();
    }

    void Append(const Event& row) {
        std::ofstream file(m_filename, std::ios::binary | std::ios::app);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + m_filename);
        }

        row.Serialize(file);
    }

    Events GetEvents() {
        Events events{};
        std::ifstream file(m_filename, std::ios::binary | std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + m_filename);
        }

        while (file.peek() != EOF) {
            Event event{};
            event.Deserialize(file);

            if (file.good()) {
                events.push_back(event);
            }
        }

        return events;
    }

    void Compact() {
        EventsMap events;
        for (auto& event : GetEvents()) {
            if (m_compactStrategies.contains(event.GetEventType())) {
                m_compactStrategies[event.GetEventType()]->Execute(events, event);
            }
        }

        RewriteHistory(events);
    }

    void Replay(RadishStore<TValue>& store) {
        for (auto events = GetEvents(); auto& event : events) {
            if (IsReplayableEvent(event) == false) {
                continue;
            }

            store.Create(*event.GetKey(), *event.GetPayload(), event.GetTimestamp());
        }
    }

private:
    std::string m_filename;
    std::string m_path;

    EventStrategies m_compactStrategies;

    bool IsReplayableEvent(const RadishEvent<TValue>& event) const {
        if (event.GetEventType() != CREATE || event.GetKey() == std::nullopt || event.GetPayload() == std::nullopt) {
            return false;
        }

        return true;
    }

    void RewriteHistory(const EventsMap& events) {
        const SystemClock clock{};
        std::ofstream file(m_filename, std::ios::binary | std::ios::trunc);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for rewriting: " + m_filename);
        }

        for (const auto& [_, event] : events) {
            if (event.GetTimestamp() != -1 && event.GetTimestamp() < clock.Now()) {
                continue;
            }

            event.Serialize(file);
        }
    }
};


#endif //RADISH_RADISHFILE_H