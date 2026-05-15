//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_RADISH_H
#define RADISH_RADISH_H

#include <optional>
#include <string>
#include <unordered_map>

#include "Recorder.h"
#include "Replayer.h"

template<typename TValue>
class Radish {
public:
    template<typename T>
    friend std::ostream& operator<<(std::ostream& os, const Radish<T>& radish);

    void Insert(const std::string& key, const TValue& value) {
        m_data[key] = value;
    }

    std::optional<TValue> Fetch(const std::string& key) {
        if (m_data.contains(key)) {
            return m_data[key];
        }

        return std::nullopt;
    }

    void Remove(const std::string& key) {
        m_data.erase(key);
    }

    void Clear() {
        m_data.clear();
    }

private:
    std::unordered_map<std::string, TValue> m_data{};
};

template<typename TValue>
std::ostream& operator<<(std::ostream& os, const Radish<TValue>& radish) {
    for (const auto& [key, value] : radish.m_data) {
        os << "\"" << key << "\" -> \"" << value << "\"" << "\n";
    }
    return os;
}

#endif //RADISH_RADISH_H