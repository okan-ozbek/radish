#include "../include/RadishDB.h"

int main() {
    RadishDB<std::string> db("my_database", 20000); // 5 seconds TTL

    return 0;
}
