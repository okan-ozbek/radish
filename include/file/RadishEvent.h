//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHROW_H
#define RADISH_RADISHROW_H


#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>

#include "Serializable.h"
#include "../helpers/Operation.h"
#include "../helpers/Types.h"

enum OperationType : unsigned char;

template<typename TValue>
requires BinaryType<TValue>
class RadishEvent final : public Serializable {
public:
    RadishEvent() = default;

    explicit RadishEvent(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key, const TValue& payload)
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
        , m_key{ key }
        , m_payload{ payload }
    {}

    explicit RadishEvent(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key)
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
        , m_key{ key }
    {}

    explicit RadishEvent(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key, const std::string& renameKey)
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
        , m_key{ key }
        , m_renameKey{ renameKey }
    {}

    explicit RadishEvent(const OperationType& operationType, const MsTimestamp& timestamp)
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
    {}

    void Serialize(std::ofstream& file) const override {
        WriteMetadata(file);

        if (m_operationType == WIPE) return;

        WriteKey(file);
        WriteRenameKey(file);

        if (m_operationType == DELETE || m_operationType == RENAME) return;

        WritePayload(file);
    }

    void Deserialize(std::ifstream& file) override {
        ReadMetadata(file);

        if (m_operationType == WIPE) return;

        ReadKey(file);
        ReadRenameKey(file);

        if (m_operationType == DELETE || m_operationType == RENAME) return;

        ReadPayload(file);
    }

    [[nodiscard]] OperationType GetOperationType() const {
        return m_operationType;
    }

    [[nodiscard]] MsTimestamp GetTimestamp() const {
        return m_timestamp;
    }

    [[nodiscard]] std::optional<std::string> GetKey() const {
        return m_key;
    }

    [[nodiscard]] std::optional<std::string> GetRenameKey() const {
        return m_renameKey;
    }

    [[nodiscard]] std::optional<TValue> GetPayload() const {
        return m_payload;
    }

private:
    OperationType m_operationType{};
    MsTimestamp m_timestamp{};
    std::optional<std::string> m_key{ std::nullopt };
    std::optional<std::string> m_renameKey{ std::nullopt };
    std::optional<TValue> m_payload{ std::nullopt };

    void WriteMetadata(std::ofstream& file) const {
        file.write(reinterpret_cast<const char*>(&m_operationType), sizeof(m_operationType));
        file.write(reinterpret_cast<const char*>(&m_timestamp), sizeof(m_timestamp));
    }

    void ReadMetadata(std::ifstream& file) {
        file.read(reinterpret_cast<char*>(&m_operationType), sizeof(m_operationType));
        file.read(reinterpret_cast<char*>(&m_timestamp), sizeof(m_timestamp));
    }

    void WriteKey(std::ofstream& file) const {
        if (m_key == std::nullopt) return;

        const BinarySize keyByteSize{ static_cast<BinarySize>(m_key->size()) };

        file.write(reinterpret_cast<const char*>(&keyByteSize), sizeof(keyByteSize));
        file.write(m_key->data(), keyByteSize);
    }

    void ReadKey(std::ifstream& file) {
        m_key = std::string{};
        BinarySize keyByteSize{};

        file.read(reinterpret_cast<char*>(&keyByteSize), sizeof(keyByteSize));

        m_key->resize(keyByteSize);
        file.read(m_key->data(), keyByteSize);
    }

    void WriteRenameKey(std::ofstream& file) const {
        if (m_operationType != RENAME || m_renameKey == std::nullopt) return;

        const BinarySize renameKeyByteSize{ static_cast<BinarySize>(m_renameKey->size()) };

        file.write(reinterpret_cast<const char*>(&renameKeyByteSize), sizeof(renameKeyByteSize));
        file.write(m_renameKey->data(), renameKeyByteSize);
    }

    void ReadRenameKey(std::ifstream& file) {
        if (m_operationType != RENAME) return;

        m_renameKey = std::string{};
        BinarySize renameKeyByteSize{};

        file.read(reinterpret_cast<char*>(&renameKeyByteSize), sizeof(renameKeyByteSize));

        m_renameKey->resize(renameKeyByteSize);
        file.read(m_renameKey->data(), renameKeyByteSize);
    }

    void WritePayload(std::ofstream& file) const {
        if (m_payload == std::nullopt) return;

        if constexpr (std::is_arithmetic_v<TValue>) {
            file.write(reinterpret_cast<const char*>(&m_payload.value()), sizeof(TValue));
        } else {
            m_payload->Serialize(file);
        }
    }

    void ReadPayload(std::ifstream& file) {
        m_payload = TValue{};
        if constexpr (std::is_arithmetic_v<TValue>) {
            file.read(reinterpret_cast<char*>(&m_payload.value()), sizeof(TValue));
        } else {
            m_payload->Deserialize(file);
        }
    }
};


#endif //RADISH_RADISHROW_H
