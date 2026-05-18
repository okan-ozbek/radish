//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHFILE_H
#define RADISH_RADISHFILE_H

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "RadishRow.h"

class Serializable;

template<typename TValue>
requires BinaryType<TValue>
class RadishFile {
public:
    RadishFile(std::string  filename, std::string  path)
        : m_filename{ std::move(filename) }
        , m_path{ std::move(path) }
    {}

    void Append(const RadishRow<TValue>& row) {
        std::ofstream file(m_filename, std::ios::binary | std::ios::app);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for writing: " + m_filename);
        }

        row.Serialize(file);
    }

    std::vector<RadishRow<TValue>> ReadAll() {
        std::vector<RadishRow<TValue>> rows{};
        std::ifstream file(m_filename, std::ios::binary | std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + m_filename);
        }

        RadishRow<TValue> row{};
        while (file.peek() != EOF) {
            row.Deserialize(file);

            if (file.good()) {
                rows.push_back(row);
            }
        }

        return rows;
    }

private:
    std::string m_filename;
    std::string m_path;
};


#endif //RADISH_RADISHFILE_H