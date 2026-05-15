//
// Created by Okan Özbek on 5/15/2026.
//

#ifndef RADISH_OPERATION_H
#define RADISH_OPERATION_H

enum OperationType {
    SET,
    DELETE,
    WIPE,
};

static std::optional<OperationType> TryGetOperationTypeByName(const std::string& name) {
    if (name == "SET")    return SET;
    if (name == "DELETE") return DELETE;
    if (name == "WIPE")   return WIPE;

    return std::nullopt;
}

static std::optional<std::string> TryGetNameByOperationType(const OperationType& operationType) {
    if (operationType == SET)    return "SET";
    if (operationType == DELETE) return "DELETE";
    if (operationType == WIPE)   return "WIPE";

    return std::nullopt;
}

#endif //RADISH_OPERATION_H