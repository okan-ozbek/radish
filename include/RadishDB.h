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
    explicit RadishDB(const std::string& name)
        : m_database{ name + ".rdh" }
        , m_recorder{ m_database }
    {
        Replayer<TValue>::Replay(m_store, m_database);
    }

    std::optional<TValue> Get(const std::string& key) {
        return m_store.Get(key);
    }

    void Set(const std::string& key, const TValue& value) {
        m_store.Set(key, value);
        m_recorder.Append(SET, key, value);
    }

    void Delete(const std::string& key) {
        m_store.Delete(key);
        m_recorder.Append(DELETE, key);
    }

    void Wipe() {
        m_store.Wipe();
        m_recorder.Append(WIPE);
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