//
// Created by Okan Özbek on 15/05/2026.
//

#ifndef RADISH_TYPES_H
#define RADISH_TYPES_H

#include <cstdint>
#include <type_traits>
#include <concepts>
#include <cstddef>

using MsTimestamp = long long;
using BinarySize = uint32_t;

class Serializable;

template<typename TValue>
concept BinaryType = std::is_arithmetic_v<TValue> || std::is_enum_v<TValue> || std::is_base_of_v<Serializable, TValue>;

template<typename TValue>
concept HeapAllocated = requires(TValue value)
{
    { value.size() } -> std::convertible_to<std::size_t>;
    { value.data() };
};

#endif //RADISH_TYPES_H