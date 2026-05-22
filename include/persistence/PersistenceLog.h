//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHFILE_H
#define RADISH_RADISHFILE_H


#include <string>

#include "LogWriter.h"
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

    PersistenceLog(const std::string& filename, const std::string& path)
        : m_writer(filename, path)
    {}

    ~PersistenceLog() {
        try {
            std::lock_guard lock{ m_mutex };

            std::cout << "Compacting log file before destruction...\n";
            m_writer.Compact();
        }
        catch (...) {
            std::cout << "Exception caught in ~LogWriter()\n";
        }
    }

    void Append(const Event& row) {
        std::lock_guard lock{ m_mutex };

        m_writer.Append(row);
    }

    void Replay(RadishStore<TValue>& store) {
        std::lock_guard lock{ m_mutex };

        m_writer.Replay(store);
    }

private:
    LogWriter<TValue> m_writer;
    mutable std::mutex m_mutex{};
};


#endif //RADISH_RADISHFILE_H