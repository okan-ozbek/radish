//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHFILE_H
#define RADISH_RADISHFILE_H

#include <string>
#include <mutex>

#include "LogWriter.h"
#include "../RadishEvent.h"

template<typename TValue> class RadishStore;

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class PersistenceLog
{
public:
    using Events          = PersistenceLogTypes<TValue>::Events;
    using Event           = PersistenceLogTypes<TValue>::Event;
    using EventsMap       = PersistenceLogTypes<TValue>::EventsMap;
    using EventStrategies = PersistenceLogTypes<TValue>::EventStrategies;

    PersistenceLog() = delete;

    PersistenceLog(const std::string& filename, const std::string& path)
        : m_writer(filename, path)
    {}

    ~PersistenceLog() = default;

    void Append(const Event& row)
    {
        std::lock_guard lock{ m_mutex };

        m_writer.Append(row);
    }

    void Replay(RadishStore<TValue>& store)
    {
        std::lock_guard lock{ m_mutex };

        m_writer.Replay(store);
    }

    [[nodiscard]]
    bool Apply(RadishStore<TValue>& store, const Event& event)
    {
        std::lock_guard lock{ m_mutex };

        return m_writer.ApplyEvent(store, event);
    }

    void Compact()
    {
        std::lock_guard lock{ m_mutex };

        m_writer.Compact();
    }

private:
    LogWriter<TValue> m_writer;
    mutable std::mutex m_mutex{};
};


#endif //RADISH_RADISHFILE_H