//
// Created by Dorza on 5/22/2026.
//

#ifndef RADISH_LOGREADER_H
#define RADISH_LOGREADER_H

#include <filesystem>
#include <fstream>
#include <array>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../helpers/Types.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class LogReader
{
public:
    using Events = PersistenceLogTypes<TValue>::Events;
    using Event  = PersistenceLogTypes<TValue>::Event;

    explicit LogReader(std::string filename)
        : m_filename{std::move( filename )}
    {}

    Events GetEvents()
    {
        Events events{};

        if (!std::filesystem::exists(m_filename)) {
            return events;
        }

        std::ifstream file(m_filename, std::ios::binary | std::ios::in);

        if (file.is_open() == false) {
            throw std::runtime_error("Failed to open file for reading: " + m_filename);
        }

        ReadHeader(file);

        while (true) {
            BinarySize recordSize{};
            file.read(reinterpret_cast<char*>(&recordSize), sizeof(recordSize));

            if (!file) {
                if (file.eof()) {
                    break;
                }

                throw std::runtime_error("Failed to read persistence record length");
            }

            if (recordSize == 0 || recordSize > kMaxRecordSize) {
                throw std::runtime_error("Invalid persistence record length");
            }

            std::vector<char> record(recordSize);
            file.read(record.data(), static_cast<std::streamsize>(record.size()));

            if (!file) {
                if (file.eof()) {
                    break;
                }

                throw std::runtime_error("Failed to read persistence record");
            }

            std::istringstream recordStream(
                std::string(record.data(), record.size()),
                std::ios::binary | std::ios::in
            );

            Event event{};
            event.Deserialize(recordStream);

            if (recordStream.peek() != EOF) {
                throw std::runtime_error("Persistence record contains trailing bytes");
            }

            events.push_back(std::move(event));
        }

        return events;
    }

private:
    static constexpr std::array<char, 8> kHeader{ 'R', 'A', 'D', 'I', 'S', 'H', '\0', '\1' };
    static constexpr BinarySize kMaxRecordSize = 64 * 1024 * 1024;

    std::string m_filename;

    static void ReadHeader(std::istream& file)
    {
        std::array<char, kHeader.size()> header{};
        ReadExact(file, header.data(), header.size());
        if (header != kHeader) {
            throw std::runtime_error("Unsupported or corrupt persistence log header");
        }
    }

    static void ReadExact(std::istream& file, char* output, std::size_t size)
    {
        file.read(output, static_cast<std::streamsize>(size));

        if (!file) {
            throw std::runtime_error("Truncated persistence log");
        }
    }
};

#endif //RADISH_LOGREADER_H