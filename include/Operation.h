//
// Created by Dorza on 5/15/2026.
//

#ifndef RADISH_OPERATION_H
#define RADISH_OPERATION_H

#include <stdexcept>

enum OperationType {
    INSERT,
    REMOVE,
    CLEAR,
};

static OperationType GetOperationTypeByName(const std::string& name) {
    if (name == "INSERT") return INSERT;
    if (name == "REMOVE") return REMOVE;
    if (name == "CLEAR") return CLEAR;

    throw std::runtime_error("Unknown operation type");
}

static std::string GetNameByOperationType(OperationType operationType) {
    if (operationType == INSERT) return "INSERT";
    if (operationType == REMOVE) return "REMOVE";
    if (operationType == CLEAR) return "CLEAR";

    throw std::runtime_error("Unknown operation type");
}

#endif //RADISH_OPERATION_H