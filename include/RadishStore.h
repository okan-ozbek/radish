//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISH_H
#define RADISH_RADISH_H


#include <optional>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "helpers/SystemClock.h"
#include "helpers/Types.h"

template<typename TValue>
class RadishStore
{
public:
    using DataMap = std::unordered_map<std::string, TValue>;
    using TTLMap = std::unordered_map<std::string, Timestamp>;
    using Keys = std::vector<std::string>;

    explicit RadishStore(const Timestamp& ttl) : m_ttl{ ttl } {}

    std::optional<TValue> Get(const std::string& key)
    {
        if (m_data.contains(key) && !IsExpired(key)) {
            return m_data.at(key);
        }

        return std::nullopt;
    }

    Timestamp Create(const std::string& key, const TValue& value, const std::optional<Timestamp> timestamp = std::nullopt)
    {
        m_data[key] = value;

        if (HasTimeToLive()) {
            return InsertTimestampForKey(key, timestamp);
        }

        return -1;
    }

    bool Rename(const std::string& oldKey, const std::string& newKey)
    {
        if (m_data.contains(oldKey) == false || IsExpired(oldKey)) {
            return false;
        }

        m_data[newKey] = m_data[oldKey];
        m_data.erase(oldKey);

        if (const auto ttlIt = m_ttlData.find(oldKey); ttlIt != m_ttlData.end()) {
            m_ttlData[newKey] = ttlIt->second;
            m_ttlData.erase(ttlIt);
        }
        else {
            m_ttlData.erase(newKey);
        }

        return true;
    }

    bool Delete(const std::string& key)
    {
        m_ttlData.erase(key);
        return m_data.erase(key) != 0;
    }

    bool Clear()
    {
        if (m_data.empty()) {
            return false;
        }

        m_data.clear();
        m_ttlData.clear();
        return true;
    }

    [[nodiscard]]
    Keys Scan() const
    {
        Keys keys{};

        for (const auto& [key, value] : m_data) {
            if (IsExpired(key)) {
                continue;
            }

            keys.push_back(key);
        }

        return keys;
    }

    [[nodiscard]]
    std::size_t Size() const
    {
        return std::count_if(m_data.begin(), m_data.end(), [this](const auto& entry) {
            return !IsExpired(entry.first);
        });
    }

    [[nodiscard]]
    bool Exists(const std::string& key) const
    {
        if (IsExpired(key)) {
            return false;
        }

        return m_data.contains(key);
    }

    [[nodiscard]]
    bool IsExpired(const std::string& key) const
    {
        if (HasTimeToLive() && m_ttlData.contains(key)) {
            return m_clock.Now() >= m_ttlData.at(key);
        }

        return false;
    }

    [[nodiscard]]
    bool HasTimeToLive() const
    {
        return m_ttl != -1;
    }

    [[nodiscard]]
    Timestamp GetTTL() const
    {
        return m_ttl;
    }

    [[nodiscard]]
    Timestamp NewExpiryTimestamp() const
    {
        return HasTimeToLive() ? m_clock.Now() + m_ttl : -1;
    }

    void Swap(RadishStore& other) noexcept
    {
        using std::swap;
        swap(m_clock, other.m_clock);
        swap(m_ttl, other.m_ttl);
        m_data.swap(other.m_data);
        m_ttlData.swap(other.m_ttlData);
    }

private:
    SystemClock m_clock{};
    Timestamp m_ttl{ -1 };

    DataMap m_data{};
    TTLMap m_ttlData{};

    Timestamp InsertTimestampForKey(const std::string& key, const std::optional<Timestamp> timestamp = std::nullopt)
    {
        const auto value = (timestamp != std::nullopt) ? timestamp.value() : m_clock.Now() + m_ttl;
        m_ttlData[key] = value;

        return value;
    }
};

#endif //RADISH_RADISH_H