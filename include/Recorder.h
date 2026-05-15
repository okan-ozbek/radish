//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_RECORDER_H
#define RADISH_RECORDER_H

#include <fstream>
#include <iosfwd>

#include "helpers/Operation.h"

template<typename TValue>
class Recorder {
public:
    using MillisecondsType = long long;

    explicit Recorder(const std::string& filename) {
        file.open(filename, std::ios::out | std::ios::app);
        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
    }

    void Append(const OperationType& operation) {
        file << GetNameByOperationType(operation) << "\n";
    }

    void Append(const OperationType& operation, const std::string& key) {
        file << GetNameByOperationType(operation) << " " << key << "\n";
    }

    void Append(const OperationType& operation, const std::string& key, const TValue& value) {
        file << GetNameByOperationType(operation) << " " << key << " " << "-1" << value << "\n";
    }

    void Append(const OperationType& operation, const std::string& key, const TValue& value, const MillisecondsType& ttl) {
        file << GetNameByOperationType(operation) << " " << key << " " << ttl << " " << value << "\n";
    }

private:
    std::ofstream file{};
};

#endif //RADISH_RECORDER_H