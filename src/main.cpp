#include <iostream>

#include "../include/RadishDB.h"

int main() {
    std::vector example = { std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }, std::byte{ 0x04 } };

    RadishDB<std::vector<std::byte>> db("mydb");

    db.Create("key1", example);

    if (const auto val = db.Get("key1"); val.value() == example) {
        std::cout << "Value retrieved successfully!" << std::endl;
    } else {
        std::cout << "Value not retrieved successfully!" << std::endl;
    }

    return std::cin.get();
}