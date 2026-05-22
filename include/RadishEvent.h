//
// Created by Dorza on 5/18/2026.
//

#ifndef RADISH_RADISHEVENT_H
#define RADISH_RADISHEVENT_H


#include <string>

#include "persistence/BinaryFile.h"
#include "enums/EventType.h"
#include "helpers/Types.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class RadishEvent {
public:
    RadishEvent() = default;

    explicit RadishEvent(
        const EventType& type,
        const Timestamp& timestamp,
        std::optional<std::string> key = std::nullopt,
        std::optional<std::string> renameKey = std::nullopt,
        std::optional<TValue> payload = std::nullopt
    )
        : m_type{ type }
        , m_timestamp{ timestamp }
        , m_key{ std::move(key) }
        , m_renameKey{ std::move(renameKey) }
        , m_payload{ std::move(payload) }
    {}

    void Serialize(std::ofstream& file) const {
        BinaryFile::Write<EventType>(file, m_type);
        BinaryFile::Write<Timestamp>(file, m_timestamp);

        if (m_type == CLEAR) return;

        BinaryFile::Write<std::string>(file, m_key);

        if (m_type == RENAME) {
            BinaryFile::Write<std::string>(file, m_renameKey);
        }

        if (m_type == DELETE || m_type == RENAME) return;

        BinaryFile::Write<TValue>(file, m_payload);
    }

    void Deserialize(std::ifstream& file) {
        BinaryFile::Read<EventType>(file, m_type);
        BinaryFile::Read<Timestamp>(file, m_timestamp);

        if (m_type == CLEAR) return;

        BinaryFile::Read<std::string>(file, m_key);

        if (m_type == RENAME) {
            BinaryFile::Read<std::string>(file, m_renameKey);
        }

        if (m_type == DELETE || m_type == RENAME) return;

        BinaryFile::Read<TValue>(file, m_payload);
    }

    [[nodiscard]] EventType GetEventType() const {
        return m_type;
    }

    [[nodiscard]] Timestamp GetTimestamp() const {
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
    EventType m_type{};
    Timestamp m_timestamp{};
    std::optional<std::string> m_key{ std::nullopt };
    std::optional<std::string> m_renameKey{ std::nullopt };
    std::optional<TValue> m_payload{ std::nullopt };
};


#endif //RADISH_RADISHEVENT_H
