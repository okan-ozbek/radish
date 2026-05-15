#include <iostream>

#include "../include/RadishDB.h"

struct Point2D {
    int x;
    int y;

    Point2D() : x(0), y(0) {}
    explicit Point2D(const int scalar) : x(scalar), y(scalar) {}
    explicit Point2D(const int x, const int y) : x(x), y(y) {}

    int Sum() const {
        return x + y;
    }
};

template<typename TValue>
struct Log {
    using MsTimestamp = uint64_t;
    using BinarySize = uint32_t;

    OperationType operationType{};
    MsTimestamp timestamp{};
    BinarySize keySize{};
    std::string key{};
    BinarySize payloadSize{};
    TValue payload{};

    Log() = default;

    Log(const OperationType& operationType, const MsTimestamp& timestamp)
        : operationType(operationType)
        , timestamp(timestamp)
    {}

    Log(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key)
        : operationType(operationType)
        , timestamp(timestamp)
        , keySize(key.size())
        , key(key)
    {}

    Log(const OperationType& operationType, const MsTimestamp& timestamp, const std::string& key, const TValue& payload)
        : operationType(operationType)
        , timestamp(timestamp)
        , keySize(key.size())
        , key(key)
        , payloadSize(sizeof(TValue))
        , payload(payload)
    {}
};

template<typename TValue>
void WriteToFile(const Log<TValue>& log) {
    std::fstream file{};

    file.open("example.radish", std::ios::out | std::ios::binary);
    if (file.is_open() == false) {
        throw std::runtime_error("Failed to open file for writing");
    }

    file.write(reinterpret_cast<const char*>(&log.operationType), sizeof(log.operationType));
    file.write(reinterpret_cast<const char*>(&log.timestamp), sizeof(log.timestamp));
    file.write(reinterpret_cast<const char*>(&log.keySize), sizeof(log.keySize));
    file.write(log.key.c_str(), log.keySize);
    file.write(reinterpret_cast<const char*>(&log.payloadSize), sizeof(log.payloadSize));
    file.write(reinterpret_cast<const char*>(&log.payload), sizeof(log.payload));
    std::cout << "Successfully written to file.\n";
}

template<typename TValue>
void ReadFromFile(Log<TValue>* log) {
    std::fstream file{};

    file.open("example.radish", std::ios::in | std::ios::binary);
    if (file.is_open() == false) {
        throw std::runtime_error("Failed to open file for reading");
    }

    file.read(reinterpret_cast<char*>(&log->operationType), sizeof(log->operationType));
    file.read(reinterpret_cast<char*>(&log->timestamp), sizeof(log->timestamp));
    file.read(reinterpret_cast<char*>(&log->keySize), sizeof(log->keySize));
    file.read(reinterpret_cast<char*>(&log->key), log->keySize);
    file.read(reinterpret_cast<char*>(&log->payloadSize), sizeof(log->payloadSize));
    file.read(reinterpret_cast<char*>(&log->payload), sizeof(log->payload));
    std::cout << "Successfully read from file.\n";
}

int main() {
    const Point2D payload{ 10, 20 };
    const Log sent{ SET, 1234567890, "point:val:1", payload };
    Log<Point2D> received{};

    WriteToFile(sent);
    ReadFromFile(&received);

    std::cout << "Operation Type: " << TryGetNameByOperationType(received.operationType).value_or("Unknown") << "\n";
    std::cout << "Timestamp: " << received.timestamp << "\n";
    std::cout << "Key: " << received.key << "\n";
    std::cout << "Payload: (" << received.payload.x << ", " << received.payload.y << ")\n";

    // RadishDB<std::string> db("my_database", 20000); // 5 seconds TTL

    std::cout << sizeof(long long) << " vs. " << sizeof(uint64_t) << std::endl;

    return 0;
}
