//
// Created by Dorza on 5/19/2026.
//

#ifndef RADISH_BINARYFILE_H
#define RADISH_BINARYFILE_H

#include <fstream>
#include <optional>

#include "Types.h"

class BinaryFile {
public:
    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Write(std::ofstream& file, const std::optional<TValue>& value) const {
        if constexpr (HeapAllocated<TValue>) {
            const BinarySize bytes{ static_cast<BinarySize>(value->size()) };

            file.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
            file.write(value->data(), bytes);

            return;
        }

        if constexpr (std::is_arithmetic_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&value.value()), sizeof(TValue));
        } else {
            value->Serialize(file);
        }
    }

    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Read(std::ifstream file, std::optional<TValue>& value) {
        if constexpr (HeapAllocated<TValue>) {
            BinarySize bytes{ static_cast<BinarySize>(value->size()) };

            file.read(reinterpret_cast<char*>(&bytes), sizeof(bytes));
            value->resize(bytes);
            file.read(value->data(), bytes);

            return;
        }

        if constexpr (std::is_arithmetic_v<TValue>) {
            file.read(reinterpret_cast<char*>(&value.value()), sizeof(TValue));
        } else {
            value->Deserialize(file);
        }
    }
};

#endif //RADISH_BINARYFILE_H