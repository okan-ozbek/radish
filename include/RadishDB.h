//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_RADISHDB_H
#define RADISH_RADISHDB_H

#include <string>
#include <iostream>

#include "Radish.h"
#include "Recorder.h"
#include "Replayer.h"

template<typename TValue>
class RadishDB {
public:
    using MillisecondsType = long long;

    explicit RadishDB(const std::string& name)
        : m_store{ -1 }
        , m_database{ name + ".rdh" }
        , m_recorder{ m_database }
    {
        Replayer<TValue>::Replay(m_store, m_database);
    }

    explicit RadishDB(const std::string& name, const MillisecondsType& ttl)
        : m_store{ ttl }
        , m_database{ name + ".rdh" }
        , m_recorder{ m_database }
    {
        Replayer<TValue>::Replay(m_store, m_database);
    }

    std::optional<TValue> Get(const std::string& key) {
        return m_store.Get(key);
    }

    void Set(const std::string& key, const TValue& value) {
        auto timestamp = m_store.Set(key, value);
        m_recorder.Append(SET, key, value, timestamp);
    }

    void Delete(const std::string& key) {
        m_store.Delete(key);
        m_recorder.Append(DELETE, key);
    }

    void Wipe() {
        m_store.Wipe();
        m_recorder.Append(WIPE);
    }

    [[nodiscard]] bool Exists(const std::string& key) const {
        return m_store.Exists(key);
    }

    [[nodiscard]] bool IsExpired(const std::string& key) const {
        return m_store.IsExpired(key);
    }

private:
    Radish<TValue> m_store;
    std::string m_database;
    Recorder<TValue> m_recorder;
};

#endif //RADISH_RADISHDB_H