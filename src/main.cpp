

#include "../include/file/RadishFile.h"
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

        name.resize(nameByteSize);
        in.read(name.data(), nameByteSize);

        in.read(reinterpret_cast<char*>(&age), sizeof(age));
    }

    void Print() const override {
        std::cout << "name: " << name << "\n";
        std::cout << "age: " << age << "\n";
    }

private:
    std::string name{};
    int age{};
};

int main() {
    RadishFile<Test> rf{ "test.radish", "./" };
    //
    // const RadishRow<int> row{ DELETE, 123, "key1", 42 };
    // const RadishRow<int> row2{ DELETE, 123, "key2", 42 };
    // const RadishRow<int> row3{ DELETE, 123, "key3", 42 };

    const RadishRow<Test> tr1{ SET, 123, "key1", Test{ "Okan", 30 } };
    const RadishRow<Test> tr2{ SET, 321, "key2", Test{ "Dorza", 25 } };

    rf.Append(tr1);
    rf.Append(tr2);

    int i{};
    for (const std::vector<RadishRow<Test>> values = rf.ReadAll(); auto& value : values) {
        std::cout << i << " -------------------\n";

        value.Print();
        ++i;
    }

    return 0;
}