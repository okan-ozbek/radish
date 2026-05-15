//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RADISH_H
#define RADISH_RADISH_H

#include <optional>
#include <string>
#include <unordered_map>

#include "helpers/SystemClock.h"
#include "helpers/Types.h"

template<typename TValue>
class Radish {
public:
    explicit Radish(const MsTimestamp& ttl) : m_ttl{ ttl } {}

    template<typename T>
    friend std::ostream& operator<<(std::ostream& os, const Radish<T>& radish);

    std::optional<TValue> Get(const std::string& key) {
        if (IsExpired(key) == false && m_data.contains(key)) {
            return m_data[key];
        }

        return std::nullopt;
    }

    MsTimestamp Set(const std::string& key, const TValue& value) {
        m_data[key] = value;

        if (!IsTTLEnabled()) {
            return -1;
        }

        const auto timestamp = m_clock.Now() + m_ttl;
        m_ttlData[key] = timestamp;

        return timestamp;
    }

    void SetByTimestamp(const std::string& key, const TValue& value, const MsTimestamp& timestamp) {
        m_data[key] = value;
        m_ttlData[key] = timestamp;
    }

    void Rename(const std::string& oldKey, const std::string& newKey) {
        if (m_data.contains(oldKey) == false) {
            return;
        }

        m_data[newKey] = m_data[oldKey];
        m_data.erase(oldKey);
    }

    void Delete(const std::string& key) {
        m_data.erase(key);
    }

    void Wipe() {
        m_data.clear();
    }

    [[nodiscard]] std::vector<std::string> Scan() const {
        std::vector<std::string> result;

        for (const auto& [key, value] : m_data) {
            if (IsExpired(key)) {
                continue;
            }

            result.push_back(key);
        }

        return result;
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
        if (IsTTLEnabled() && m_ttlData.contains(key)) {
            return m_clock.Now() >= m_ttlData.at(key);
        }

        return false;
    }

    [[nodiscard]] bool IsTTLEnabled() const {
        return m_ttl != -1;
    }

    [[nodiscard]] MsTimestamp GetTTL() const {
        return m_ttl;
    }

private:
    SystemClock m_clock{};
    MsTimestamp m_ttl{ -1 };

    std::unordered_map<std::string, TValue> m_data{};
    std::unordered_map<std::string, MsTimestamp> m_ttlData{};
};

template<typename TValue>
std::ostream& operator<<(std::ostream& os, const Radish<TValue>& radish) {
    for (const auto& [key, value] : radish.m_data) {
        os << "\"" << key << "\" -> \"" << value << "\"" << "\n";
    }
    return os;
}

#endif //RADISH_RADISH_H