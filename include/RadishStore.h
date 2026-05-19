//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISH_H
#define RADISH_RADISH_H

#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

#include "helpers/SystemClock.h"
#include "helpers/Types.h"

template<typename TValue>
class RadishStore {
public:
    using DataMap = std::unordered_map<std::string, TValue>;
    using TTLMap = std::unordered_map<std::string, Timestamp>;
    using Keys = std::vector<std::string>;

    explicit RadishStore(const Timestamp& ttl) : m_ttl{ ttl } {}

    std::optional<TValue> Get(const std::string& key) {
        if (m_data.contains(key) && !IsExpired(key)) {
            return m_data.at(key);
        }

        return std::nullopt;
    }

    Timestamp Create(const std::string& key, const TValue& value, const std::optional<Timestamp> timestamp = std::nullopt) {
        m_data[key] = value;

        if (HasTimeToLive()) {
            return InsertTimestampForKey(key, timestamp);
        }

        return -1;
    }

    void Rename(const std::string& oldKey, const std::string& newKey) {
        if (m_data.contains(oldKey) == false || IsExpired(oldKey)) {
            return;
        }

        m_data[newKey] = m_data[oldKey];
        m_ttlData[newKey] = m_ttlData[oldKey];

        m_data.erase(oldKey);
        m_ttlData.erase(oldKey);
    }

    void Delete(const std::string& key) {
        m_data.erase(key);
    }

    void Clear() {
        m_data.clear();
    }

    [[nodiscard]] Keys Scan() const {
        Keys keys{};

        for (const auto& [key, value] : m_data) {
            if (IsExpired(key)) {
                continue;
            }

            keys.push_back(key);
        }

        return keys;
    }

    [[nodiscard]] std::size_t Size() const {
        return m_data.size();
    }

    [[nodiscard]] bool Exists(const std::string& key) const {
        if (IsExpired(key)) {
            return false;
        }

        return m_data.contains(key);
    }

    [[nodiscard]] bool IsExpired(const std::string& key) const {
        if (HasTimeToLive() && m_ttlData.contains(key)) {
            return m_clock.Now() >= m_ttlData.at(key);
        }

        return false;
    }

    [[nodiscard]] bool HasTimeToLive() const {
        return m_ttl != -1;
    }

    [[nodiscard]] Timestamp GetTTL() const {
        return m_ttl;
    }

private:
    SystemClock m_clock{};
    Timestamp m_ttl{ -1 };

    DataMap m_data{};
    TTLMap m_ttlData{};

    Timestamp InsertTimestampForKey(const std::string& key, const std::optional<Timestamp> timestamp = std::nullopt) {
        const auto value = (timestamp != std::nullopt) ? timestamp.value() : m_clock.Now() + m_ttl;
        m_ttlData[key] = value;

        return value;
    }
};

#endif //RADISH_RADISH_H