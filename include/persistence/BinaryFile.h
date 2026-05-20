//
// Created by Dorza on 5/19/2026.
//

#ifndef RADISH_BINARYFILE_H
#define RADISH_BINARYFILE_H

#include <fstream>
#include <optional>

#include "../helpers/Concepts.h"

class BinaryFile {
public:
    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Write(std::ofstream& file, const TValue& value) {
        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;
            static_assert(std::is_trivially_copyable_v<ElementType>, "HeapAllocated element type must be trivially copyable");

            const BinarySize bytes{ static_cast<BinarySize>(value.size() * sizeof(ElementType)) };

            file.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
            file.write(reinterpret_cast<const char*>(value.data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(TValue));
        }
        else {
            value.Serialize(file);
        }
    }

    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Write(std::ofstream& file, const std::optional<TValue>& value) {
        if (value == std::nullopt) return;

        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;
            static_assert(std::is_trivially_copyable_v<ElementType>, "HeapAllocated element type must be trivially copyable");

            const BinarySize bytes{ static_cast<BinarySize>(value->size() * sizeof(ElementType)) };

            file.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
            file.write(reinterpret_cast<const char*>(value->data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&value.value()), sizeof(TValue));
        }
        else {
            value->Serialize(file);
        }
    }

    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Read(std::ifstream& file, TValue& value) {
        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;
            static_assert(std::is_trivially_copyable_v<ElementType>, "HeapAllocated element type must be trivially copyable");

            BinarySize bytes{};
            file.read(reinterpret_cast<char*>(&bytes), sizeof(bytes));
            value.resize(bytes / sizeof(ElementType));
            file.read(reinterpret_cast<char*>(value.data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.read(reinterpret_cast<char*>(&value), sizeof(TValue));
        }
        else {
            value.Deserialize(file);
        }
    }

    template<typename TValue>
    requires BinaryType<TValue> || HeapAllocated<TValue>
    static void Read(std::ifstream& file, std::optional<TValue>& value) {
        if (value == std::nullopt) {
            value = TValue{};
        }

        if constexpr (HeapAllocated<TValue>) {
            using ElementType = TValue::value_type;
            static_assert(std::is_trivially_copyable_v<ElementType>, "HeapAllocated element type must be trivially copyable");

            BinarySize bytes{};
            file.read(reinterpret_cast<char*>(&bytes), sizeof(bytes));
            value->resize(bytes / sizeof(ElementType));
            file.read(reinterpret_cast<char*>(value->data()), bytes);
        }
        else if constexpr (std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>) {
            file.read(reinterpret_cast<char*>(&value.value()), sizeof(TValue));
        }
        else {
            value->Deserialize(file);
        }
    }
};

#endif //RADISH_BINARYFILE_H