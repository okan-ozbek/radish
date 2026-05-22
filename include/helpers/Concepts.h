//
// Created by Dorza on 5/19/2026.
//

#ifndef RADISH_CONCEPTS_H
#define RADISH_CONCEPTS_H

#include <type_traits>
#include <concepts>
#include <cstddef>

template<typename TValue>
concept BinaryType = std::is_arithmetic_v<TValue> || std::is_enum_v<TValue>;

template<typename TValue>
concept HeapAllocated = requires(TValue value)
{
    { value.size() } -> std::convertible_to<std::size_t>;
    { value.data() };
};


#endif //RADISH_CONCEPTS_H