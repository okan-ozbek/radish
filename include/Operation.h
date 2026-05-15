//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_OPERATION_H
#define RADISH_OPERATION_H

#include <stdexcept>

enum OperationType {
    SET,
    DELETE,
    WIPE,
};

static OperationType GetOperationTypeByName(const std::string& name) {
    if (name == "SET")    return SET;
    if (name == "DELETE") return DELETE;
    if (name == "WIPE")   return WIPE;

    throw std::runtime_error("Unknown operation type");
}

static std::string GetNameByOperationType(OperationType operationType) {
    if (operationType == SET)    return "SET";
    if (operationType == DELETE) return "DELETE";
    if (operationType == WIPE)   return "WIPE";

    throw std::runtime_error("Unknown operation type");
}

#endif //RADISH_OPERATION_H