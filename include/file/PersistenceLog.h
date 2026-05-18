//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHFILE_H
#define RADISH_RADISHFILE_H


#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

#include "RadishEvent.h"

class Serializable;

template<typename TValue>
class RadishStore;

template<typename TValue>
requires BinaryType<TValue>
class PersistenceLog {
public:
    PersistenceLog(std::string  filename, std::string  path)
        : m_filename{ std::move(filename) }
        , m_path{ std::move(path) }
    {}

    void Append(const RadishEvent<TValue>& row) {
        std::ofstream file(m_filename, std::ios::binary | std::ios::app);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + m_filename);
        }

        row.Serialize(file);
    }

    std::vector<RadishEvent<TValue>> GetEvents() {
        std::vector<RadishEvent<TValue>> rows{};
        std::ifstream file(m_filename, std::ios::binary | std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + m_filename);
        }

        while (file.peek() != EOF) {
            RadishEvent<TValue> row{};
            row.Deserialize(file);

            if (file.good()) {
                rows.push_back(row);
            }
        }

        return rows;
    }

    void Replay(RadishStore<TValue>& store) {
        for (auto events = GetEvents(); auto& event : events) {
            switch (event.GetOperationType()) {
                case SET:
                    store.SetByTimestamp(event.GetKey().value(), event.GetPayload().value(), event.GetTimestamp());
                    break;
                case RENAME:
                    store.Rename(event.GetKey().value(), event.GetRenameKey().value());
                    break;
                case DELETE:
                    store.Delete(event.GetKey().value());
                    break;
                case WIPE:
                    store.Wipe();
                    break;
                default:
                    throw std::runtime_error("Unknown operation type encountered during replay.");
            }
        }
    }

private:
    std::string m_filename;
    std::string m_path;
};


#endif //RADISH_RADISHFILE_H