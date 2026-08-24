//
// Created by Dorza on 5/19/2026.
//

#ifndef RADISH_BINARYFILE_H
#define RADISH_BINARYFILE_H

#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>

#include "../helpers/Concepts.h"
#include "../helpers/Types.h"

class BinaryFile {
public:
    template<typename TValue, typename TStream>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Write(TStream& file, const TValue& value) {
        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;

            if (value.size() > std::numeric_limits<BinarySize>::max() / sizeof(ElementType)) {
                throw std::length_error("Binary value is too large to serialize");
            }

            const BinarySize bytes{ static_cast<BinarySize>(value.size() * sizeof(ElementType)) };

            file.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
            file.write(reinterpret_cast<const char*>(value.data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(TValue));
        }

        if (!file) {
            throw std::ios_base::failure("Failed to write binary value");
        }
    }

    template<typename TValue, typename TStream>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Write(TStream& file, const std::optional<TValue>& value) {
        if (!value) {
            throw std::invalid_argument("Cannot serialize a missing required field");
        }

        Write(file, *value);
    }

    template<typename TValue, typename TStream>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Read(TStream& file, TValue& value) {
        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;

            BinarySize bytes{};
            file.read(reinterpret_cast<char*>(&bytes), sizeof(bytes));

            if (!file || bytes % sizeof(ElementType) != 0) {
                throw std::runtime_error("Invalid binary value length");
            }

            value.resize(bytes / sizeof(ElementType));
            file.read(reinterpret_cast<char*>(value.data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.read(reinterpret_cast<char*>(&value), sizeof(TValue));
        }

        if (!file) {
            throw std::runtime_error("Unexpected end of binary value");
        }
    }

    template<typename TValue, typename TStream>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Read(TStream& file, std::optional<TValue>& value) {
        if (value == std::nullopt) {
            value = TValue{};
        }

        Read(file, *value);
    }
};

#endif //RADISH_BINARYFILE_H