//
// Created by Okan Özbek on 15/05/2026.
//

#ifndef RADISH_TYPES_H
#define RADISH_TYPES_H


using MsTimestamp = long long;
using BinarySize = uint32_t;

class Serializable;

template<typename TValue>
concept BinaryType = std::is_arithmetic_v<TValue> || std::is_base_of_v<Serializable, TValue>;

#endif //RADISH_TYPES_H