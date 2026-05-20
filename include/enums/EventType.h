//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_EVENTTYPE_H
#define RADISH_EVENTTYPE_H

#include <optional>
#include <string>
#include <cstdint>

enum EventType : uint8_t {
    CREATE = 0,
    RENAME = 1,
    DELETE = 2,
    CLEAR  = 3,
};

static std::optional<EventType> TryGetEventTypeByName(const std::string& name) {
    if (name == "CREATE") return CREATE;
    if (name == "RENAME") return RENAME;
    if (name == "DELETE") return DELETE;
    if (name == "CLEAR")  return CLEAR;

    return std::nullopt;
}

static std::optional<std::string> TryGetNameByEventType(const EventType& type) {
    if (type == CREATE) return "CREATE";
    if (type == RENAME) return "RENAME";
    if (type == DELETE) return "DELETE";
    if (type == CLEAR)  return "CLEAR";

    return std::nullopt;
}

#endif //RADISH_EVENTTYPE_H