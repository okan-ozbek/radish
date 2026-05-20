//
//
//

#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Database {
public:
    using Key = std::string;
    using Value = std::string;
    using Datas = std::unordered_map<Key, Value>;

    Database() = default;

    void Insert(const Key& key, const Value& value) {
        std::lock_guard lock(m_mutex);
        m_data[key] = value;
    }

    void Insert(Key&& key, Value&& value) {
        std::lock_guard lock(m_mutex);

        m_data[std::move(key)] = std::move(value);
    }

    void Delete(const Key& key) {
        std::lock_guard lock(m_mutex);
        m_data.erase(key);
    }

    std::optional<Value> Get(const Key& key) const {
        std::shared_lock lock(m_mutex);

        if (const auto it = m_data.find(key); it != m_data.end()) {
            return it->second;
        }

        return std::nullopt;
    }

private:
    mutable std::shared_mutex m_mutex;

    Datas m_data{};
};

int main() {
    Database db{};

    constexpr int iterations = 100000;

    std::vector<std::thread> threads;

    // writers
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int j = 0; j < iterations; ++j)
            {
                std::cout << "Inserting: " << j << std::endl;
                db.Insert("key", std::to_string(j));
            }
        });
    }

    // readers
    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int j = 0; j < iterations; ++j)
            {
                auto value = db.Get("key");

                if (value.has_value())
                {
                    std::cout << value.value() << std::endl;
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    return 0;
}
