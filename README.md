```
 ███████████                 █████  ███          █████     
▒▒███▒▒▒▒▒███               ▒▒███  ▒▒▒          ▒▒███      
 ▒███    ▒███   ██████    ███████  ████   █████  ▒███████  
 ▒██████████   ▒▒▒▒▒███  ███▒▒███ ▒▒███  ███▒▒   ▒███▒▒███ 
 ▒███▒▒▒▒▒███   ███████ ▒███ ▒███  ▒███ ▒▒█████  ▒███ ▒███ 
 ▒███    ▒███  ███▒▒███ ▒███ ▒███  ▒███  ▒▒▒▒███ ▒███ ▒███ 
 █████   █████▒▒████████▒▒████████ █████ ██████  ████ █████
▒▒▒▒▒   ▒▒▒▒▒  ▒▒▒▒▒▒▒▒  ▒▒▒▒▒▒▒▒ ▒▒▒▒▒ ▒▒▒▒▒▒  ▒▒▒▒ ▒▒▒▒▒ 
```
**A Redis-inspired, persistent key-value store — built from scratch in C++23**

![C++](https://img.shields.io/badge/C%2B%2B-23-blue?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/Build-CMake-red?logo=cmake&logoColor=white)
![License](https://img.shields.io/badge/License-Copyright-red)
![Status](https://img.shields.io/badge/Status-In%20Progress-yellow)
![Purpose](https://img.shields.io/badge/Purpose-Self%20Learning-brightgreen)

---

## About

Redis is one of the most battle-tested pieces of infrastructure software ever written. It powers caching layers,
session stores, message queues, leaderboards, and rate limiters at companies operating at enormous scale. At its
core, however, the concept is disarmingly simple: a global hash map in memory, a write-ahead log for durability,
and an expressive command interface on top.

**Radish** is a ground-up reimplementation of that core idea in modern C++23. The goal is not to replace Redis
or match its performance — it is to fully understand what it takes to build a durable, in-memory key-value store
from first principles: how persistence works, how state is replayed after a crash, how concerns like data storage
and I/O should be structured, and what it means to design a system that is extensible without becoming brittle.

> The goal is: **understand, from the ground up, how a production key-value store is architected.**

---

## Use of AI

This project uses **AI as an assistant**, not as the author. The code is **hand-written**. AI is used when stuck
on a C++ concept, to debug issues, or to get unstuck on nuances like template instantiation errors, include
guard collisions, or stream mode flags.

The **documentation** is largely AI-generated because AI can explain concepts in a structured way, and the focus
of this project is on the implementation — not the write-ups.

> **TL;DR:** The code is written by hand. AI helps explain concepts and document them.

---

## Architecture

Radish is designed around a strict **separation of concerns**. Each layer has one responsibility and no knowledge
of the layers above it.

```
RadishDB<TValue>                  -- Public facade: composes store + persistence
    RadishStore<TValue>           -- Pure in-memory key-value store with TTL (no I/O)
    PersistenceLog<TValue>        -- Binary AOF: append events, replay on startup
        RadishEvent<TValue>       -- Typed event: operation + timestamp + key + payload
            Serializable          -- Base class for custom serialisable value types
            BinaryFile            -- Static read/write helpers (arithmetic, heap, Serializable)
        helpers/Operation.h       -- OperationType enum + TryGet* helpers
        helpers/Types.h           -- MsTimestamp, BinarySize, BinaryType, HeapAllocated concepts
        helpers/SystemClock.h     -- Clock abstraction for TTL expiry checks
```

`RadishStore` can be instantiated and tested with zero file system involvement. The persistence layer is
entirely opt-in through `RadishDB`. `PersistenceLog` owns the full read/write/replay lifecycle — `RadishDB`
calls `m_database.Replay(m_store)` and nothing more.

---

## Current Implementation

### Key-Value Store — `RadishStore<TValue>`

A generic in-memory store backed by `std::unordered_map<std::string, TValue>`. Accepts any value type
that satisfies the `BinaryType` or `HeapAllocated` concept.

TTL is opt-in: construct with a millisecond duration to enable expiry, or pass `-1` for a persistent-forever
store. Expired keys are checked lazily on access — no background thread required.

| Method | Redis Equivalent | Description |
|---|---|---|
| `Set(key, value)` | `SET key value` | Insert or overwrite. Returns expiry timestamp, or `-1` if TTL disabled |
| `SetByTimestamp(key, value, ts)` | — | Internal: restores a key with a known expiry timestamp during replay |
| `Get(key)` | `GET key` | Returns `std::optional<TValue>`. Returns `nullopt` if missing or expired |
| `Delete(key)` | `DEL key` | Remove a key |
| `Wipe()` | `FLUSHDB` | Remove all keys |
| `Rename(old, new)` | `RENAME old new` | Atomically move a value to a new key. No-op if old key is missing or expired |
| `Exists(key)` | `EXISTS key` | Returns `false` if the key is missing or expired |
| `Scan()` | `KEYS *` | Returns all live (non-expired) keys as `std::vector<std::string>` |
| `Size()` | `DBSIZE` | Returns total number of stored keys |
| `IsExpired(key)` | `TTL key` (partial) | Returns `true` if the key has a TTL and it has passed |
| `IsTTLEnabled()` | — | Returns `true` if the store was constructed with a TTL duration |
| `GetTTL()` | `TTL key` (partial) | Returns the configured TTL duration in milliseconds |

### Persistence — Binary Append-Only File

Every write operation is serialised as a `RadishEvent` and appended to a `.radish` binary file. On startup,
`PersistenceLog::Replay` reads all events and applies them to the store in order to reconstruct state.

The binary format is operation-driven — only fields relevant to the operation are written:

| Operation | Fields written |
|---|---|
| `SET` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `payload` |
| `DELETE` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` |
| `RENAME` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `renameKeyLen (4B)` + `renameKey` |
| `WIPE` | `opType (1B)` + `timestamp (8B)` |

No delimiters or sentinels. The operation type byte tells the reader exactly which fields to expect next —
making it both compact and unambiguous.

- **`PersistenceLog<TValue>`** — opens in append mode on `Append`, reads in binary mode on `GetEvents`.
  Owns the `Replay(RadishStore<TValue>&)` method: iterates events and applies them to the store. `RadishDB`
  calls this with one line and has zero knowledge of the event format.
- **`RadishEvent<TValue>`** — typed event object. Implements `Serializable`. Serialises/deserialises itself
  using `BinaryFile` helpers or its own `if constexpr` dispatch for each field.

### Event Model — `RadishEvent<TValue>`

`RadishEvent` is the unit of persistence. It holds:
- `OperationType` — `SET`, `DELETE`, `RENAME`, or `WIPE`
- `MsTimestamp` — absolute millisecond expiry timestamp (stored so TTL survives a restart)
- `std::optional<std::string>` key and rename key
- `std::optional<TValue>` payload

Serialisation dispatches at compile time based on `TValue`:
- **Arithmetic / enum** — raw `file.read` / `file.write` of fixed width
- **`HeapAllocated`** (e.g. `std::string`, `std::vector`) — length-prefix then bytes
- **`Serializable`** subclass — delegates to `value.Serialize(file)` / `value.Deserialize(file)`

### Binary I/O Helper — `BinaryFile`

A static utility class with templated `Read` and `Write` overloads. Centralises the `if constexpr` dispatch
so the same logic is not duplicated across every serialisable type:

```cpp
BinaryFile::Write(file, key);       // length-prefixed string
BinaryFile::Write(file, timestamp); // raw 8-byte integer
BinaryFile::Write(file, payload);   // dispatches to Serialize() for custom types
```

The `HeapAllocated` concept in `Types.h` matches any type with `size()` and `data()`:

```cpp
template<typename TValue>
concept HeapAllocated = requires(TValue t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.data() };
};
```

### Serialisable Values — `Serializable`

To store a custom struct as a value in `RadishDB`, inherit from `Serializable` and implement three methods:

```cpp
class MyType final : public Serializable {
public:
    void Serialize(std::ofstream& out) const override { /* write fields */ }
    void Deserialize(std::ifstream& in) override      { /* read fields  */ }
    void Print() const override                        { /* debug print  */ }
};

RadishDB<MyType> db("my_database", 30000);
db.Set("key1", MyType{ ... });
```

### Operations — `helpers/Operation.h`

Defines the `OperationType` enum and two helpers returning `std::optional` instead of throwing:

```
SET     -- Write a key/value with an expiry timestamp
DELETE  -- Remove a key
WIPE    -- Clear the entire store
RENAME  -- Move a value to a new key
```

### Public Interface — `RadishDB<TValue>`

```cpp
// No TTL — keys live forever
RadishDB<std::string> db("my_database");

db.Set("users:id:1", "Alice");
db.Set("users:id:2", "Bob");
db.Delete("users:id:1");
db.Rename("users:id:2", "users:id:99");

auto keys = db.Scan();  // { "users:id:99" }
auto size = db.Size();  // 1

// On next run: state is automatically restored from my_database.radish
```

```cpp
// With TTL — keys expire after 5000ms
RadishDB<std::string> db("my_database", 5000);

db.Set("session:abc", "user_42");  // expires in 5 seconds
db.Exists("session:abc");          // true (within TTL)
// ... 5 seconds later ...
db.Exists("session:abc");          // false (expired)
```

```cpp
// Custom Serializable type
RadishDB<MyType> db("my_database", 30000);
db.Set("key1", MyType{ "Okan", 30 });

auto val = db.Get("key1");  // restored from binary file on next run
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| **Templates over inheritance** | Allows the store to hold any type without virtual dispatch or type erasure overhead |
| **`std::optional` for `Get`** | Avoids sentinel values and exceptions for the normal "key not found" case |
| **`std::optional` for error handling** | `TryGet*` helpers return `std::nullopt` on unknown input — no exceptions in hot paths |
| **Binary format over text AOF** | No delimiters to parse, no ambiguity, smaller on disk, faster to write and replay |
| **Operation-driven serialisation** | Only fields relevant to the operation are written — `WIPE` writes 9 bytes total, not a padded record |
| **`RadishEvent` as typed event model** | Each event carries its own type, timestamp, key, and payload — self-describing and easy to replay |
| **`BinaryFile` as centralised I/O** | One place for `if constexpr` dispatch — arithmetic, heap-allocated, and `Serializable` all handled uniformly |
| **`Serializable` base class** | Allows custom value types to participate in binary persistence with minimal boilerplate |
| **`HeapAllocated` concept** | Compile-time detection of types with `size()` + `data()` — covers `std::string`, `std::vector`, and any compatible type |
| **`PersistenceLog::Replay` owns replay** | `RadishDB` calls one method. The switch over operation types lives in `PersistenceLog`, not in the facade |
| **RAII for file handles** | Files are opened per-operation and closed on scope exit — no persistent handle state to manage |
| **Timestamp stored in event** | Expiry survives a restart: the absolute `MsTimestamp` is written so `SetByTimestamp` can restore it exactly |
| **Lazy expiry** | Expired keys are checked at access time — no background thread, no complexity |
| **`Rename` as atomic operation** | Implemented directly in `m_data` without a public `Delete` + `Set` round-trip |
| **Snapshot before move in `Set(TValue&&)`** | The value is captured before `std::move` so the event written to disk has the full payload, not a moved-from empty object |

---

## Roadmap

### Short Term — Completeness

All short-term features are complete.

| Feature | Status | Notes |
|---|---|---|
| **Key TTL / Expiry** | Done | Absolute timestamp stored per key. `Get`, `Exists`, `Scan` all respect TTL. Timestamp written to binary log and restored on replay. |
| **`Scan()`** | Done | Returns all live keys. Mirrors `KEYS *`. |
| **`Rename(old, new)`** | Done | Atomic in `m_data`. Recorded as `RENAME` event in binary log. |
| **`Size()`** | Done | `m_data.size()`. |
| **Error handling** | Done | `TryGet*` return `std::optional`. Unknown events skipped silently during replay. |

### Medium Term — Robustness

| Feature | Status | Description |
|---|---|---|
| **Binary serialisation** | In Progress | `RadishEvent` serialises to binary. `BinaryFile` centralises `if constexpr` dispatch. `PersistenceLog` reads/writes in `std::ios::binary`. Remaining: route all field I/O through `BinaryFile` consistently, harden edge cases. |
| **AOF Compaction** | Planned | Rewrite the log to its minimal equivalent: remove superseded `SET` calls, drop keys that were `DELETE`d. Prevents unbounded log growth. |
| **Snapshot persistence (RDB-like)** | Planned | Serialise the full in-memory store to a compact binary file on a schedule. Faster startup than replaying a long log. Run alongside AOF like Redis: snapshot for fast restore, AOF for durability. |
| **`std::variant` value types** | Planned | `std::variant<std::string, int64_t, double>` as the value type for heterogeneous storage. Requires a type discriminator byte in `RadishEvent`. |
| **Unit tests (GoogleTest)** | Planned | Round-trip tests: write events, re-open, verify state matches. Test TTL expiry, rename, wipe. Isolate `RadishStore`, `PersistenceLog`, and `BinaryFile` independently. |
| **Thread safety** | Planned | `std::shared_mutex` on `m_data`: concurrent reads, exclusive writes. Prerequisite for any networking work. |

### Long Term — From Toy to System

| Feature | Description |
|---|---|
| **TCP Server** | Wrap `RadishDB` in a TCP server using standalone Asio (no Boost). Accept connections and dispatch commands — making Radish something you can `telnet` into. |
| **RESP Protocol** | Implement the Redis Serialisation Protocol so that real Redis clients (`redis-cli`, client libraries) talk to Radish without modification. |
| **List type** | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`. Backed by `std::deque`. |
| **Set type** | `SADD`, `SREM`, `SMEMBERS`, `SINTER`. Backed by `std::unordered_set`. |
| **Pub/Sub** | Clients subscribe to channels, other clients publish messages. Requires the networking layer first. |
| **Benchmarking** | Measure throughput (ops/sec) and latency (p50/p99) against real Redis on equivalent workloads. Make the numbers visible and quantifiable. |

---

## Project Structure

```
radish/
├── include/
│   ├── helpers/
│   │   ├── BinaryFile.h        # Static Read/Write helpers (arithmetic, HeapAllocated, Serializable)
│   │   ├── Operation.h         # OperationType enum + TryGet* helpers (std::optional)
│   │   ├── SystemClock.h       # Clock abstraction for TTL expiry checks
│   │   └── Types.h             # MsTimestamp, BinarySize, BinaryType, HeapAllocated concepts
│   ├── file/
│   │   ├── PersistenceLog.h    # Binary AOF: Append, GetEvents, Replay
│   │   ├── RadishEvent.h       # Typed event: op + timestamp + key + payload (Serializable)
│   │   └── Serializable.h      # Base class for custom serialisable value types
│   ├── RadishStore.h           # Pure in-memory key-value store with TTL
│   └── RadishDB.h              # Public facade: composes RadishStore + PersistenceLog
├── src/
│   └── main.cpp                # Entry point
└── README.md
```

---

## Building

Requires CMake 4.0+ and a C++23 compiler.

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
./cmake-build-debug/radish
```

---

## Built With

- **Language:** C++23
- **Build System:** CMake 4.0
- **IDE:** CLion
- **Compiler:** MinGW (GCC)

---

## License

Copyright &copy; 2026 Okan Ozbek. All rights reserved.
