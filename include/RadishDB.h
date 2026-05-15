//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_RADISHDB_H
#define RADISH_RADISHDB_H

#include <string>

#include "Radish.h"
#include "Recorder.h"
#include "Replayer.h"

template<typename TValue>
class RadishDB {
public:
    explicit RadishDB(const std::string& name)
        : m_database{ name + ".rdh" }
        , m_recorder{ m_database }
    {
        Replayer<TValue>::Replay(m_store, m_database);
    }

    void Insert(const std::string& key, const TValue& value) {
        m_store.Insert(key, value);
        m_recorder.Append(INSERT, key, value);
    }

    void Remove(const std::string& key) {
        m_store.Remove(key);
        m_recorder.Append(REMOVE, key);
    }

    void Clear() {
        m_store.Clear();
        m_recorder.Append(CLEAR);
    }

    void Print() {
        std::cout << m_store << std::endl;
    }

private:
    Radish<TValue> m_store{};

    std::string m_database;
    Recorder<TValue> m_recorder;
};

#endif //RADISH_RADISHDB_H