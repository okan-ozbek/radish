//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHROW_H
#define RADISH_RADISHROW_H


#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

#include "Serializable.h"

enum OperationType : unsigned char;

template<typename TValue>
concept BinaryType = std::is_arithmetic_v<TValue> || std::is_base_of_v<Serializable, TValue>;

template<typename TValue>
requires BinaryType<TValue>
class RadishRow final : public Serializable {
public:
    using MsTimestamp = uint64_t;
    using BinarySize = uint32_t;

    RadishRow() = default;

    explicit RadishRow(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key, const TValue& payload)
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
        , m_key{ key }
        , m_payload{ payload }
    {}

    void Serialize(std::ofstream& file) const override {
        file.write(reinterpret_cast<const char*>(&m_operationType), sizeof(m_operationType));
        file.write(reinterpret_cast<const char*>(&m_timestamp), sizeof(m_timestamp));

        const BinarySize keyByteSize{ static_cast<BinarySize>(m_key.size()) };
        file.write(reinterpret_cast<const char*>(&keyByteSize), sizeof(keyByteSize));
        file.write(m_key.data(), keyByteSize);

        if constexpr (std::is_arithmetic_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&m_payload), sizeof(m_payload));
        } else {
            m_payload.Serialize(file);
        }
    }

    void Deserialize(std::ifstream& file) override {
        file.read(reinterpret_cast<char*>(&m_operationType), sizeof(m_operationType));
        file.read(reinterpret_cast<char*>(&m_timestamp), sizeof(m_timestamp));

        BinarySize keyByteSize{};
        file.read(reinterpret_cast<char*>(&keyByteSize), sizeof(keyByteSize));

        m_key.resize(keyByteSize);
        file.read(m_key.data(), keyByteSize);

        if constexpr (std::is_arithmetic_v<TValue>) {
            file.read(reinterpret_cast<char*>(&m_payload), sizeof(m_payload));
        } else {
            m_payload.Deserialize(file);
        }
    }

    void Print() const {
        std::cout << "Operation Type: " << static_cast<int>(m_operationType) << "\n";
        std::cout << "Timestamp: " << m_timestamp << "\n";
        std::cout << "Key: " << m_key << "\n";

        if constexpr (std::is_arithmetic_v<TValue>) {
            std::cout << "Payload: " << m_payload << "\n";
        } else {
            m_payload.Print();
        }
    }

private:
    OperationType m_operationType{};
    MsTimestamp m_timestamp{};
    std::string m_key{};
    TValue m_payload{};
};

#endif //RADISH_RADISHROW_H
