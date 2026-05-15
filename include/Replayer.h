//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_REPLAYER_H
#define RADISH_REPLAYER_H

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "helpers/Operation.h"

template<typename TValue>
class Radish;

template<typename TValue>
class Replayer {
public:
    using MillisecondsType = long long;

    static void Replay(Radish<TValue>& instance, const std::string& databaseName) {
        std::ifstream file{};
        file.open(databaseName, std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + databaseName);
        }

        std::string line{};
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string operation{}, key{}, value{};
            MillisecondsType timestamp{};

            stream >> operation >> key >> timestamp >> value;

            OperationType type = GetOperationTypeByName(operation);
            if (type == SET)    instance.SetByTimestamp(key, value, timestamp);
            if (type == DELETE) instance.Delete(key);
            if (type == WIPE)   instance.Wipe();
        }
    }
};


#endif //RADISH_REPLAYER_H