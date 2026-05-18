//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHEVENT_H
#define RADISH_RADISHEVENT_H


#include <string>

#include "Serializable.h"
#include "../helpers/BinaryFile.h"
#include "../helpers/Operation.h"
#include "../helpers/Types.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class RadishEvent final : public Serializable {
public:
    RadishEvent() = default;

    explicit RadishEvent(
        const OperationType& operationType,
        const MsTimestamp& timestamp,
        std::optional<std::string> key = std::nullopt,
        std::optional<std::string> renameKey = std::nullopt,
        std::optional<TValue> payload = std::nullopt
    )
        : m_operationType{ operationType }
        , m_timestamp{ timestamp }
        , m_key{ std::move(key) }
        , m_renameKey{ std::move(renameKey) }
        , m_payload{ std::move(payload) }
    {}

    void Serialize(std::ofstream& file) const override {
        BinaryFile::Write<OperationType>(file, m_operationType);
        BinaryFile::Write<MsTimestamp>(file, m_timestamp);

        if (m_operationType == WIPE) return;

        BinaryFile::Write<std::string>(file, m_key);

        if (m_operationType == RENAME) {
            BinaryFile::Write<std::string>(file, m_renameKey);
        }

        if (m_operationType == DELETE || m_operationType == RENAME) return;

        BinaryFile::Write<TValue>(file, m_payload);
    }

    void Deserialize(std::ifstream& file) override {
        BinaryFile::Read<OperationType>(file, m_operationType);
        BinaryFile::Read<MsTimestamp>(file, m_timestamp);

        if (m_operationType == WIPE) return;

        BinaryFile::Read<std::string>(file, m_key);

        if (m_operationType == RENAME) {
            BinaryFile::Read<std::string>(file, m_renameKey);
        }

        if (m_operationType == DELETE || m_operationType == RENAME) return;

        BinaryFile::Read<TValue>(file, m_payload);
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
};


#endif //RADISH_RADISHEVENT_H
