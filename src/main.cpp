#include "../include/RadishDB.h"

int main() {
    RadishDB<std::string> db("my_database");

    db.Set("user:1", "Alice");
    db.Set("user:2", "Bob");
    db.Print();
    db.Delete("user:1");
    db.Print();
    db.Wipe();
    db.Print();

    return 0;
}
