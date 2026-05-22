//
// Created by Dorza on 5/22/2026.
//

#ifndef RADISH_LOGREADER_H
#define RADISH_LOGREADER_H

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "../helpers/Types.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class LogReader {
public:
    using Events = PersistenceLogTypes<TValue>::Events;
    using Event  = PersistenceLogTypes<TValue>::Event;

    LogReader(const std::string& filename, const std::string& path)
        : m_filename{ filename }
        , m_path{ path }
    {}

    Events GetEvents() {
        Events events{};

        if (!std::filesystem::exists(m_filename)) {
            std::ofstream create(m_filename, std::ios::binary);
            return events;
        }

        std::ifstream file(m_filename, std::ios::binary | std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + m_filename);
        }

        while (file.peek() != EOF) {
            Event event{};
            event.Deserialize(file);

            if (file.good()) {
                events.push_back(event);
            }
        }

        return events;
    }

private:
    std::string m_filename;
    std::string m_path;
};

#endif //RADISH_LOGREADER_H