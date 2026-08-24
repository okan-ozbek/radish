//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISHDB_H
#define RADISH_RADISHDB_H

#include <shared_mutex>
#include <string>
#include <optional>
#include <stdexcept>

#include "RadishStore.h"
#include "persistence/PersistenceLog.h"

enum class PersistenceMode {
    Flush,
    Disabled,
};

template<typename TValue>
class RadishDB
{
public:
    explicit RadishDB(const std::string& filename, const PersistenceMode persistenceMode = PersistenceMode::Flush)
        : m_store{ -1 }
    {
        InitializePersistence(filename, persistenceMode);
    }

    explicit RadishDB(
        const std::string& filename,
        const Timestamp& ttl,
        const PersistenceMode persistenceMode = PersistenceMode::Flush
    )
        : m_store{ ttl }
    {
        InitializePersistence(filename, persistenceMode);
    }

    void Create(const std::string& key, const TValue& value)
    {
        std::lock_guard lock{ m_mutex };

        Commit(RadishEvent<TValue>{ CREATE, m_store.NewExpiryTimestamp(), key, std::nullopt, value });
    }

    bool Rename(const std::string& oldKey, const std::string& newKey)
    {
        std::lock_guard lock{ m_mutex };

        return Commit(RadishEvent<TValue>{ RENAME, -1, oldKey, newKey });
    }

    bool Delete(const std::string& key)
    {
        std::lock_guard lock{ m_mutex };

        return Commit(RadishEvent<TValue>{ DELETE, -1, key });
    }

    bool Clear()
    {
        std::lock_guard lock{ m_mutex };

        return Commit(RadishEvent<TValue>{ CLEAR, -1 });
    }

    void Compact()
    {
        std::lock_guard lock{ m_mutex };

        if (m_persistence) {
            m_persistence->Compact();
        }
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
    bool Commit(const RadishEvent<TValue>& event)
    {
        auto stagedStore = m_store;
        const auto applied = m_persistence
            ? m_persistence->Apply(stagedStore, event)
            : ApplyEvent(stagedStore, event);

        if (!applied) {
            return false;
        }

        if (m_persistence) {
            m_persistence->Append(event);
        }
        m_store.Swap(stagedStore);
        return true;
    }

    void InitializePersistence(const std::string& filename, const PersistenceMode persistenceMode)
    {
        if (persistenceMode == PersistenceMode::Disabled) {
            return;
        }

        m_persistence.emplace(filename + ".radish", "./");
        m_persistence->Replay(m_store);
    }

    static bool ApplyEvent(RadishStore<TValue>& store, const RadishEvent<TValue>& event)
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
                throw std::runtime_error("Unknown persistence event type");
        }
    }

    RadishStore<TValue> m_store;
    std::optional<PersistenceLog<TValue>> m_persistence;
    mutable std::shared_mutex m_mutex{};
};

#endif //RADISH_RADISHDB_H