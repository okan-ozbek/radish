

#include "../include/RadishDB.h"
#include "../include/file/PersistenceLog.h"
#include "../include/helpers/Operation.h"

// class Test final : public Serializable {
// public:
//     Test() = default;
//
//     explicit Test(std::string name, int age)
//         : name{ std::move(name) }
//         , age{ age }
//     {}
//
//     void Serialize(std::ofstream &out) const override {
//         const uint32_t nameByteSize{ static_cast<uint32_t>(name.size()) };
//         out.write(reinterpret_cast<const char*>(&nameByteSize), sizeof(nameByteSize));
//         out.write(name.data(), nameByteSize);
//
//         out.write(reinterpret_cast<const char*>(&age), sizeof(age));
//     }
//
//     void Deserialize(std::ifstream &in) override {
//         uint32_t nameByteSize{};
//         in.read(reinterpret_cast<char*>(&nameByteSize), sizeof(nameByteSize));
//
//         name.resize(nameByteSize);
//         in.read(name.data(), nameByteSize);
//
//         in.read(reinterpret_cast<char*>(&age), sizeof(age));
//     }
//
//     void Print() {
//         std::cout << "name: " << name << ", age: " << age << "\n";
//     }
//
// private:
//     std::string name{};
//     int age{};
// };
//
// int main() {
//     RadishDB<Test> db{ "test", 25000 };
//     db.Set("key1", Test{ "Okan", 30 });
//     auto key1 = db.Get("key1");
//     if (key1 != std::nullopt) {
//         std::cout << "Key1 exists:\n";
//         key1->Print();
//     } else {
//         std::cout << "Key1 does not exist or is expired.\n";
//     }
//
//     return 0;
// }

enum EventType : uint8_t {
    CREATE = 0,
    UPDATE = 1,
};

class SimpleEvent {
public:
    explicit SimpleEvent(const EventType& type) : m_type{ type } {}

    [[nodiscard]] EventType GetType() const {
        return m_type;
    }

private:
    EventType m_type;
};

class EventStrategy {
public:
    virtual ~EventStrategy() = default;

    virtual void HandleEvent(const SimpleEvent& event) = 0;
};

class CreateEventStrategy final : public EventStrategy {
public:
    ~CreateEventStrategy() override = default;

    void HandleEvent(const SimpleEvent& event) override {
        std::cout << "Called the create event!" << std::endl;
    }
};

class UpdateEventStrategy final : public EventStrategy {
public:
    ~UpdateEventStrategy() override = default;

    void HandleEvent(const SimpleEvent& event) override {
        std::cout << "Called the update event!" << std::endl;
    }
};

class Handler {
public:
    Handler() {
        m_strategies[CREATE] = std::make_unique<CreateEventStrategy>();
        m_strategies[UPDATE] = std::make_unique<UpdateEventStrategy>();
    }

    void Execute(const SimpleEvent& event) {
        if (m_strategies.contains(event.GetType())) {
            m_strategies[event.GetType()]->HandleEvent(event);
        }
    }

private:
    std::unordered_map<EventType, std::unique_ptr<EventStrategy>> m_strategies;
};

int main() {
    Handler handler;
    handler.Execute(SimpleEvent{ CREATE });

    return 0;
}

