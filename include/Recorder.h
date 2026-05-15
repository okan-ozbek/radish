//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_RECORDER_H
#define RADISH_RECORDER_H

#include <fstream>
#include <iosfwd>

#include "helpers/Operation.h"

class Recorder {
public:
    explicit Recorder(const std::string& filename) {
        file.open(filename, std::ios::out | std::ios::app);
        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
    }

    template<typename ...TArgs>
    void TryAppend(const OperationType& operation, TArgs&&... args) {
        const auto result = TryGetNameByOperationType(operation);
        if (result == std::nullopt) {
            return;
        }

        file << *result;
        ((file << " " << std::forward<TArgs>(args)), ...);
        file << "\n";
    }

private:
    std::ofstream file{};


};

#endif //RADISH_RECORDER_H