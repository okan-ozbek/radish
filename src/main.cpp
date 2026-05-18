

#include "../include/RadishDB.h"
#include "../include/file/PersistenceLog.h"
#include "../include/helpers/Operation.h"

class Test final : public Serializable {
public:
    Test() = default;

    explicit Test(std::string name, int age)
        : name{ std::move(name) }
        , age{ age }
    {}

    void Serialize(std::ofstream &out) const override {
        const uint32_t nameByteSize{ static_cast<uint32_t>(name.size()) };
        out.write(reinterpret_cast<const char*>(&nameByteSize), sizeof(nameByteSize));
        out.write(name.data(), nameByteSize);

        out.write(reinterpret_cast<const char*>(&age), sizeof(age));
    }

    void Deserialize(std::ifstream &in) override {
        uint32_t nameByteSize{};
        in.read(reinterpret_cast<char*>(&nameByteSize), sizeof(nameByteSize));

        std::cout << "Deserializing: " << nameByteSize << " bytes\n";

        name.resize(nameByteSize);
        in.read(name.data(), nameByteSize);

        in.read(reinterpret_cast<char*>(&age), sizeof(age));
    }

    void Print() {
        std::cout << "name: " << name << ", age: " << age << "\n";
    }

private:
    std::string name{};
    int age{};
};

int main() {
    RadishDB<Test> db{ "test", 25000 };
    db.Set("key1", Test{ "Okan", 30 });
    auto key1 = db.Get("key1");
    if (key1 != std::nullopt) {
        std::cout << "Key1 exists:\n";
        key1->Print();
    } else {
        std::cout << "Key1 does not exist or is expired.\n";
    }

    return 0;
}