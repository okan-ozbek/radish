//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RECORDER_H
#define RADISH_RECORDER_H

#include <fstream>
#include <iosfwd>

#include "helpers/Operation.h"

template<typename TValue>
class Recorder {
public:
    explicit Recorder(const std::string& filename) {
        file.open(filename, std::ios::out | std::ios::app);
        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
    }

    void Append(const OperationType& operation) {
        TryAppend(operation);
    }

    void Append(const OperationType& operation, const std::string& key) {
        TryAppend(operation, key);
    }

    void Append(const OperationType& operation, const std::string& key, const TValue& value) {
        TryAppend(operation, key, value, -1);
    }

    void Append(const OperationType& operation, const std::string& key, const TValue& value, const MsType& ttl) {
        TryAppend(operation, key, value, ttl);
    }

private:
    std::ofstream file{};

    template<typename ...TArgs>
    void TryAppend(OperationType operation, TArgs&&... args) {
        const auto result = TryGetNameByOperationType(operation);
        if (result == std::nullopt) {
            return;
        }

        file << *result;
        ((file << " " << std::forward<TArgs>(args)), ...);
        file << "\n";
    }
};

#endif //RADISH_RECORDER_H