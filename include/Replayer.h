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

// TODO temp this class should be replaced with proper binary encoding for better memory and performance management
template<typename TValue>
class Replayer {
public:
    static void Replay(Radish<TValue>& instance, const std::string& databaseName) {
        std::ifstream file{};
        file.open(databaseName, std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + databaseName);
        }

        std::string line{};
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string operation{}, key{}, value{}, newKey{};
            MsTimestamp timestamp{};

            stream >> operation;

            const auto result = TryGetOperationTypeByName(operation);
            if (result == std::nullopt) {
                continue;
            }

            switch (*result) {
                case SET:
                    stream >> key >> timestamp >> value;
                    instance.SetByTimestamp(key, value, timestamp);
                    break;
                case RENAME:
                    stream >> key >> newKey;
                    instance.Rename(key, newKey);
                    break;
                case DELETE:
                    stream >> key;
                    instance.Delete(key);
                    break;
                case WIPE:
                    instance.Wipe();
                    break;
            }
        }
    }
};


#endif //RADISH_REPLAYER_H