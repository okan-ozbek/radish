//
// Created by Okan Özbek on 15/05/2026.
//

#ifndef RADISH_TYPES_H
#define RADISH_TYPES_H

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

#include "Concepts.h"

template<typename TValue>
requires BinaryType<TValue> || HeapAllocated<TValue>
class RadishEvent;

template<typename TValue>
class CompactStrategy;

enum EventType : uint8_t;

using Timestamp = long long;
using BinarySize = uint32_t;

template<typename TValue>
struct PersistenceLogTypes {
    using Events          = std::vector<RadishEvent<TValue>>;
    using Event           = RadishEvent<TValue>;
    using EventsMap       = std::unordered_map<std::string, RadishEvent<TValue>>;
    using EventStrategies = std::unordered_map<EventType, std::unique_ptr<CompactStrategy<TValue>>>;
};

#endif //RADISH_TYPES_H