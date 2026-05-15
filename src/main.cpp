#include "../include/RadishDB.h"

int main() {
    RadishDB<std::string> db("my_database", 20000); // 5 seconds TTL

    if (db.Exists("key1")) {
        std::cout << "key1 exists" << std::endl;
    } else {
        std::cout << "key1 does not exist" << std::endl;
    }

    const auto keys = db.Scan();
    for (const auto& key : keys) {
        std::cout << key << std::endl;
    }

    return 0;
}
