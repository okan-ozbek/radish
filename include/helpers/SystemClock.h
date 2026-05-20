//
// Created by Okan Özbek on 15/05/2026.
//

#ifndef RADISH_TIMER_H
#define RADISH_TIMER_H

#include <chrono>

#include "Types.h"

struct IClock {
    [[nodiscard]] virtual Timestamp Now() const = 0;
    virtual ~IClock() = default;
};

struct SystemClock final : IClock {
    [[nodiscard]] Timestamp Now() const override {
        const auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
};

#endif //RADISH_TIMER_H