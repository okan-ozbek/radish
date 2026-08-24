//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISHDB_H
#define RADISH_RADISHDB_H

#include <shared_mutex>
#include <string>

#include "RadishStore.h"
#include "persistence/PersistenceLog.h"

template<typename TValue>
class RadishDB
{
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

    void Create(const std::string& key, const TValue& value)
    {
        std::lock_guard lock{ m_mutex };

        Commit(RadishEvent<TValue>{ CREATE, m_store.NewExpiryTimestamp(), key, std::nullopt, value });
    }

    void Rename(const std::string& oldKey, const std::string& newKey)
    {
        std::lock_guard lock{ m_mutex };

        Commit(RadishEvent<TValue>{ RENAME, -1, oldKey, newKey });
    }

    void Delete(const std::string& key)
    {
        std::lock_guard lock{ m_mutex };

        Commit(RadishEvent<TValue>{ DELETE, -1, key });
    }

    void Clear()
    {
        std::lock_guard lock{ m_mutex };

        Commit(RadishEvent<TValue>{ CLEAR, -1 });
    }

    void Compact()
    {
        std::lock_guard lock{ m_mutex };

        m_persistence.Compact();
    }

    std::optional<TValue> Get(const std::string& key)
    {
        std::shared_lock lock{ m_mutex };

        return m_store.Get(key);
    }

    [[nodiscard]]
    std::vector<std::string> Scan() const
    {
        std::shared_lock lock{ m_mutex };

        return m_store.Scan();
    }

    [[nodiscard]]
    std::size_t Size() const
    {
        std::shared_lock lock{ m_mutex };

        return m_store.Size();
    }

    [[nodiscard]]
    bool Exists(const std::string& key) const {
        std::shared_lock lock{ m_mutex };

        return m_store.Exists(key);
    }

    [[nodiscard]]
    bool IsExpired(const std::string& key) const
    {
        std::shared_lock lock{ m_mutex };

        return m_store.IsExpired(key);
    }

private:
    void Commit(const RadishEvent<TValue>& event)
    {
        auto stagedStore = m_store;
        if (!m_persistence.Apply(stagedStore, event)) {
            return;
        }

        m_persistence.Append(event);
        m_store.Swap(stagedStore);
    }

    RadishStore<TValue> m_store;
    PersistenceLog<TValue> m_persistence;
    mutable std::shared_mutex m_mutex{};
};

#endif //RADISH_RADISHDB_H