//
// Created by Dorza on 5/19/2026.
//

#ifndef RADISH_COMPACTSTRATEGY_H
#define RADISH_COMPACTSTRATEGY_H


#include <string>
#include <unordered_map>

#include "../RadishEvent.h"

template<typename TValue>
class CompactStrategy {
public:
    virtual ~CompactStrategy() = default;

    virtual void Execute(std::unordered_map<std::string, RadishEvent<TValue>>&, const RadishEvent<TValue>&) = 0;
};

// TODO: Can move to separate file it grows to large

template<typename TValue>
class CreateCompactStrategy final : public CompactStrategy<TValue> {
public:
    ~CreateCompactStrategy() override = default;

    void Execute(std::unordered_map<std::string, RadishEvent<TValue>>& events, const RadishEvent<TValue>& event) override {
        events[*event.GetKey()] = event;
    }
};

template<typename TValue>
class RenameCompactStrategy final : public CompactStrategy<TValue> {
public:
    ~RenameCompactStrategy() override = default;

    void Execute(std::unordered_map<std::string, RadishEvent<std::string>>& events, const RadishEvent<std::string>& event) override {
        if (const auto it = events.find(event.GetKey().value()); it != events.end()) {
            const auto entry = it->second;
            events.erase(it);

            events[event.GetRenameKey().value()] = entry;
        }
    }
};

template<typename TValue>
class DeleteCompactStrategy final : public CompactStrategy<TValue> {
public:
    ~DeleteCompactStrategy() override = default;

    void Execute(std::unordered_map<std::string, RadishEvent<TValue>>& events, const RadishEvent<TValue>& event) override {
        events.erase(event.GetKey().value());
    }
};

template<typename TValue>
class ClearCompactStrategy final : public CompactStrategy<TValue> {
public:
    ~ClearCompactStrategy() override = default;

    void Execute(std::unordered_map<std::string, RadishEvent<TValue>>& events, const RadishEvent<TValue>& event) override {
        events.clear();
    }
};


#endif //RADISH_COMPACTSTRATEGY_H