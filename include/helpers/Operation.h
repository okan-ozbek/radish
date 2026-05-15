//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_OPERATION_H
#define RADISH_OPERATION_H

#include <optional>
#include <string>

enum OperationType : uint8_t {
    SET    = 0,
    DELETE = 1,
    WIPE   = 2,
    RENAME = 3,
};

static std::optional<OperationType> TryGetOperationTypeByName(const std::string& name) {
    if (name == "SET")    return SET;
    if (name == "DELETE") return DELETE;
    if (name == "WIPE")   return WIPE;
    if (name == "RENAME") return RENAME;

    return std::nullopt;
}

static std::optional<std::string> TryGetNameByOperationType(const OperationType& operationType) {
    if (operationType == SET)    return "SET";
    if (operationType == DELETE) return "DELETE";
    if (operationType == WIPE)   return "WIPE";
    if (operationType == RENAME) return "RENAME";

    return std::nullopt;
}

#endif //RADISH_OPERATION_H