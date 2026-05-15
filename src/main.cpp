#include "../include/RadishDB.h"

int main() {
    RadishDB<std::string> db("my_database", 20000); // 5 seconds TTL

    db.Set("key1", "value1");

    if (db.Exists("key1")) {
        std::cout << "key1 exists" << std::endl;
    } else {
        std::cout << "key1 does not exist" << std::endl;
    }

    for (const auto keys = db.Scan(); const auto& key : keys) {
        std::cout << key << std::endl;
    }

    return 0;
}
