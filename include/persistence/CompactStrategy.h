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
    using Event     = PersistenceLogTypes<TValue>::Event;
    using EventsMap = PersistenceLogTypes<TValue>::EventsMap;

    virtual ~CompactStrategy() = default;

    virtual void Execute(EventsMap&, const Event&) = 0;
};

// TODO: Can move to separate file it grows to large

template<typename TValue>
class CreateCompactStrategy final : public CompactStrategy<TValue> {
public:
    using Event = RadishEvent<TValue>;
    using EventsMap = std::unordered_map<std::string, RadishEvent<TValue>>;

    ~CreateCompactStrategy() override = default;

    void Execute(EventsMap& events, const Event& event) override {
        events[*event.GetKey()] = event;
    }
};

template<typename TValue>
class RenameCompactStrategy final : public CompactStrategy<TValue> {
public:
    using Event = RadishEvent<TValue>;
    using EventsMap = std::unordered_map<std::string, RadishEvent<TValue>>;

    ~RenameCompactStrategy() override = default;

    void Execute(EventsMap& events, const Event& event) override {
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
    using Event = RadishEvent<TValue>;
    using EventsMap = std::unordered_map<std::string, RadishEvent<TValue>>;

    ~DeleteCompactStrategy() override = default;

    void Execute(EventsMap& events, const Event& event) override {
        events.erase(event.GetKey().value());
    }
};

template<typename TValue>
class ClearCompactStrategy final : public CompactStrategy<TValue> {
public:
    using Event = RadishEvent<TValue>;
    using EventsMap = std::unordered_map<std::string, RadishEvent<TValue>>;

    ~ClearCompactStrategy() override = default;

    void Execute(EventsMap& events, const Event& event) override {
        events.clear();
    }
};


#endif //RADISH_COMPACTSTRATEGY_H