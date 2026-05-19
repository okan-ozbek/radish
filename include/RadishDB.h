//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISHDB_H
#define RADISH_RADISHDB_H

#include <string>

#include "RadishStore.h"
#include "file/PersistenceLog.h"

template<typename TValue>
class RadishDB {
public:
    explicit RadishDB(const std::string& filename)
        : m_store{ -1 }
        , m_persistence{ filename + ".radish", "./" }
    {
        m_persistence.Replay(m_store);
    }

    explicit RadishDB(const std::string& filename, const Timestamp& ttl)
        : m_store{ ttl }
        , m_persistence{ filename + ".radish", "./" }
    {
        m_persistence.Replay(m_store);
    }

    std::optional<TValue> Get(const std::string& key) {
        return m_store.Get(key);
    }

    void Create(const std::string& key, const TValue& value) {
        auto timestamp = m_store.Create(key, value);
        m_persistence.Append(RadishEvent<TValue>{ CREATE, timestamp, key, std::nullopt, value });
    }

    void Rename(const std::string& oldKey, const std::string& newKey) {
        m_store.Rename(oldKey, newKey);
        m_persistence.Append(RadishEvent<TValue>{ RENAME, 0, oldKey, newKey });
    }

    void Delete(const std::string& key) {
        m_store.Delete(key);
        m_persistence.Append(RadishEvent<TValue>{ DELETE, 0, key });
    }

    void Clear() {
        m_store.Wipe();
        m_persistence.Append(RadishEvent<TValue>{ CLEAR, 0 });
    }

    [[nodiscard]] std::vector<std::string> Scan() const {
        return m_store.Scan();
    }

    [[nodiscard]] std::size_t Size() const {
        return m_store.Size();
    }

    [[nodiscard]] bool Exists(const std::string& key) const {
        return m_store.Exists(key);
    }

    [[nodiscard]] bool IsExpired(const std::string& key) const {
        return m_store.IsExpired(key);
    }

private:
    RadishStore<TValue> m_store;
    PersistenceLog<TValue> m_persistence;
};

#endif //RADISH_RADISHDB_H