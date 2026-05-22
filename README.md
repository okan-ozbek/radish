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
RadishDB<TValue>                  -- Public facade: composes store + persistence + shared_mutex
    RadishStore<TValue>           -- Pure in-memory key-value store with TTL (no I/O)
    PersistenceLog<TValue>        -- Thread-safe AOF facade: mutex + delegates to LogWriter
        LogWriter<TValue>         -- Append, Replay, Compact; owns a LogReader internally
            LogReader<TValue>     -- Binary file reader: deserialises events from disk
        RadishEvent<TValue>       -- Typed event: op + timestamp + key + payload
            Serializable          -- Pure-virtual base: Serialize + Deserialize
            BinaryFile            -- Static R/W helpers (arithmetic, HeapAllocated, Serializable)
        CompactStrategy<TValue>   -- Strategy pattern: per-operation compaction logic
        enums/EventType.h         -- EventType enum (uint8_t) + TryGet* helpers
        helpers/Types.h           -- Timestamp, BinarySize type aliases
        helpers/Concepts.h        -- BinaryType, HeapAllocated concepts + Serializable forward decl
        helpers/SystemClock.h     -- IClock interface + SystemClock (chrono-backed)
```

`RadishStore` can be instantiated and tested with zero file system involvement. The persistence layer is
entirely opt-in through `RadishDB`. `PersistenceLog` is a thin thread-safe facade over `LogWriter`, which
owns `Append`, `Replay`, and `Compact`. `LogReader` is kept as a separate, single-responsibility class so
that read and write paths can evolve independently — `RadishDB` calls `m_persistence.Replay(m_store)` and
nothing more.

---

## Current Implementation

### Key-Value Store — `RadishStore<TValue>`

A generic in-memory store backed by `std::unordered_map<std::string, TValue>`. Accepts any value type
that satisfies the `BinaryType` or `HeapAllocated` concept.

TTL is opt-in: construct with a millisecond duration to enable expiry, or pass `-1` for a persistent-forever
store. Expired keys are checked lazily on access — no background thread required.

| Method | Redis Equivalent | Description |
|---|---|---|
| `Create(key, value)` | `SET key value` | Insert or overwrite. Returns expiry timestamp, or `-1` if TTL disabled |
| `Create(key, value, ts)` | — | Internal: restores a key with a known expiry timestamp during replay |
| `Get(key)` | `GET key` | Returns `std::optional<TValue>`. Returns `nullopt` if missing or expired |
| `Delete(key)` | `DEL key` | Remove a key |
| `Clear()` | `FLUSHDB` | Remove all keys |
| `Rename(old, new)` | `RENAME old new` | Atomically move a value to a new key. No-op if old key is missing or expired |
| `Exists(key)` | `EXISTS key` | Returns `false` if the key is missing or expired |
| `Scan()` | `KEYS *` | Returns all live (non-expired) keys as `std::vector<std::string>` |
| `Size()` | `DBSIZE` | Returns total number of stored keys |
| `IsExpired(key)` | `TTL key` (partial) | Returns `true` if the key has a TTL and it has passed |
| `HasTimeToLive()` | — | Returns `true` if the store was constructed with a TTL duration |
| `GetTTL()` | `TTL key` (partial) | Returns the configured TTL duration in milliseconds |

### Persistence — Binary Append-Only File

Every write operation is serialised as a `RadishEvent` and appended to a `.radish` binary file. On startup,
`PersistenceLog::Replay` reads all events and applies them to the store in order to reconstruct state. On
shutdown, `PersistenceLog::Compact` rewrites the file to its minimal equivalent automatically.

The binary format is operation-driven — only fields relevant to the operation are written:

| Operation | Fields written |
|---|---|
| `CREATE` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `payload` |
| `DELETE` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` |
| `RENAME` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `renameKeyLen (4B)` + `renameKey` |
| `CLEAR` | `opType (1B)` + `timestamp (8B)` |

No delimiters or sentinels. The operation type byte tells the reader exactly which fields to expect next —
making it both compact and unambiguous.

The persistence layer is split into three focused classes:

- **`PersistenceLog<TValue>`** — thin, thread-safe facade. Owns a `std::mutex`, forwards `Append`,
  `Replay`, and `Compact` to `LogWriter`. Destructor calls `Compact()` automatically on clean shutdown.
- **`LogWriter<TValue>`** — owns the write path entirely: `Append` (single event append), `Replay` (apply
  all events to a store), and `Compact` (reduce the log to its minimal equivalent). Internally holds a
  `LogReader` to read back events during replay and compaction.
- **`LogReader<TValue>`** — owns the read path: opens the file in binary mode and deserialises events
  one at a time. Creates the file if it does not exist. Kept separate so the read path can evolve without
  touching write logic.
- **`RadishEvent<TValue>`** — typed event object. Implements `Serializable`. Serialises/deserialises itself
  using `BinaryFile` helpers with compile-time `if constexpr` dispatch per field.

### AOF Compaction — `CompactStrategy<TValue>`

On shutdown, `PersistenceLog` reduces the log to its minimal equivalent using the Strategy pattern. Each
`EventType` has a dedicated strategy that knows how to apply that operation to a live key map:

| Strategy | Behaviour |
|---|---|
| `CreateCompactStrategy` | Inserts the event into the map under its key (latest `CREATE` wins) |
| `RenameCompactStrategy` | Moves the existing entry to the new key name |
| `DeleteCompactStrategy` | Erases the key from the map |
| `ClearCompactStrategy` | Clears the entire map |

After all strategies have been applied, `RewriteHistory` truncates the file and writes only the surviving
`CREATE` events — skipping any whose TTL has already expired. The result is a file that contains exactly the
state that would be produced by replaying the full original log, in the minimum number of bytes.

The strategy pattern keeps compaction logic isolated per operation — adding a new `EventType` requires only
a new strategy class, with no changes to `PersistenceLog` itself.

### Event Model — `RadishEvent<TValue>`

`RadishEvent` is the unit of persistence. It holds:
- `EventType` — `CREATE`, `DELETE`, `RENAME`, or `CLEAR`
- `Timestamp` — absolute millisecond expiry timestamp (stored so TTL survives a restart)
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

The `HeapAllocated` concept in `Concepts.h` matches any type with `size()` and `data()`:

```cpp
template<typename TValue>
concept HeapAllocated = requires(TValue t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.data() };
};
```

A `static_assert` on `std::is_trivially_copyable_v<ElementType>` guards against accidentally storing
non-trivial element types (e.g. `std::vector<std::string>`) via the raw byte path.

### Serialisable Values — `Serializable`

To store a custom struct as a value in `RadishDB`, inherit from `Serializable` and implement two methods:

```cpp
class MyType final : public Serializable {
public:
    void Serialize(std::ofstream& out) const override {
        // write each field using BinaryFile::Write or raw file.write
    }

    void Deserialize(std::ifstream& in) override {
        // read each field back in the same order
    }
};

RadishDB<MyType> db("my_database", 30000);
db.Create("key1", MyType{ ... });
```

`Serializable` does **not** require a `Print` method — that is left to the concrete type.

### Clock Abstraction — `helpers/SystemClock.h`

TTL expiry checks go through an `IClock` interface, making the clock injectable and testable:

```cpp
struct IClock {
    [[nodiscard]] virtual Timestamp Now() const = 0;
};

struct SystemClock final : IClock {
    [[nodiscard]] Timestamp Now() const override; // std::chrono::system_clock
};
```

`RadishStore` holds a `SystemClock` by value. In tests this could be replaced with a mock clock to
control expiry without real time passing.

### Operations — `enums/EventType.h`

Defines the `EventType` enum and two helpers returning `std::optional` instead of throwing:

```
CREATE  -- Write a key/value with an expiry timestamp
DELETE  -- Remove a key
CLEAR   -- Clear the entire store
RENAME  -- Move a value to a new key
```

### Public Interface — `RadishDB<TValue>`

```cpp
// No TTL — keys live forever
RadishDB<std::string> db("my_database");

db.Create("users:id:1", "Alice");
db.Create("users:id:2", "Bob");
db.Delete("users:id:1");
db.Rename("users:id:2", "users:id:99");

auto keys = db.Scan();  // { "users:id:99" }
auto size = db.Size();  // 1

// On next run: state is automatically restored from my_database.radish
```

```cpp
// With TTL — keys expire after 5000ms
RadishDB<std::string> db("my_database", 5000);

db.Create("session:abc", "user_42");  // expires in 5 seconds
db.Exists("session:abc");             // true (within TTL)
// ... 5 seconds later ...
db.Exists("session:abc");             // false (expired)
```

```cpp
// Custom Serializable type
RadishDB<MyType> db("my_database", 30000);
db.Create("key1", MyType{ "Okan", 30 });

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
| **Operation-driven serialisation** | Only fields relevant to the operation are written — `CLEAR` writes 9 bytes total, not a padded record |
| **`RadishEvent` as typed event model** | Each event carries its own type, timestamp, key, and payload — self-describing and easy to replay |
| **`BinaryFile` as centralised I/O** | One place for `if constexpr` dispatch — arithmetic, heap-allocated, and `Serializable` all handled uniformly |
| **`Serializable` base class** | Allows custom value types to participate in binary persistence with minimal boilerplate |
| **`HeapAllocated` concept** | Compile-time detection of types with `size()` + `data()` — covers `std::string`, `std::vector<T>`, and any compatible type. `sizeof(ElementType)` ensures correct byte count for non-char element types. `static_assert` on `is_trivially_copyable` prevents silent corruption. |
| **`Concepts.h` separate from `Types.h`** | `Types.h` has zero dependencies (pure aliases). `Concepts.h` owns concept definitions and forward-declares `Serializable` — preventing circular includes between `helpers/` and `file/` layers. |
| **Strategy pattern for compaction** | Each `EventType` gets an isolated strategy class. Adding a new operation requires only a new strategy — `PersistenceLog` and `Compact()` need no changes. |
| **Compaction on destructor** | `~PersistenceLog()` calls `Compact()` — the file is always left in minimal form on clean shutdown, so the next startup replays the smallest possible log. If the process crashes, the full AOF is still intact and correct. |
| **`IClock` interface on `SystemClock`** | `RadishStore` depends on `IClock`, not the concrete clock — making TTL logic mockable in future unit tests without real time passing |
| **`PersistenceLog::Replay` owns replay** | `RadishDB` calls one method. The switch over operation types lives in `PersistenceLog`, not in the facade |
| **`PersistenceLog` as a thin facade** | `PersistenceLog` owns only a mutex and delegates everything to `LogWriter`. Thread-safety concerns are isolated in one place and do not bleed into read or write logic. |
| **`LogReader` / `LogWriter` separation** | Read and write paths are independent classes. `LogWriter` can evolve its write strategy or compaction logic without touching the deserialisation code in `LogReader`, and vice versa. |
| **`std::shared_mutex` on `RadishDB`** | Concurrent reads (`Get`, `Scan`, `Exists`, etc.) use `shared_lock` so they run in parallel. Writes (`Create`, `Delete`, etc.) use exclusive `lock_guard`. Two independent mutexes — one in `RadishDB`, one in `PersistenceLog` — keep store and I/O state protected separately. |
| **Timestamp stored in event** | Expiry survives a restart: the absolute `Timestamp` is written so `Create(key, value, ts)` can restore it exactly |
| **Lazy expiry** | Expired keys are checked at access time — no background thread, no complexity |
| **`Rename` as atomic operation** | Implemented directly in `m_data` without a public `Delete` + `Create` round-trip |
| **Snapshot before move in `Create(TValue&&)`** | The value is captured before `std::move` so the event written to disk has the full payload, not a moved-from empty object |

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
| **Binary serialisation** | Done | `RadishEvent` fully routes all field I/O through `BinaryFile::Read` / `BinaryFile::Write`. Four overloads cover raw values and `std::optional<TValue>` for both read and write. `static_assert` guards non-trivially-copyable element types. `PersistenceLog` opens files in `std::ios::binary`. |
| **AOF Compaction** | Done | Strategy pattern per `EventType`. `Compact()` simulates full replay into a live key map, then `RewriteHistory` truncates and rewrites only surviving `CREATE` events — skipping expired keys. Called automatically in `~PersistenceLog()`. |
| **LogReader / LogWriter split** | Done | Read and write paths are now separate classes. `LogWriter` owns `Append`, `Replay`, and `Compact`; `LogReader` owns event deserialisation from disk. `PersistenceLog` is a thin mutex-guarded facade over `LogWriter`. |
| **Thread safety** | Done | `RadishDB` uses `std::shared_mutex`: `shared_lock` for concurrent reads (`Get`, `Scan`, `Size`, `Exists`, `IsExpired`), exclusive `lock_guard` for writes. `PersistenceLog` adds a second `std::mutex` guarding file I/O, so the two layers protect their own state independently. |
| **`std::variant` value types** | Planned | `std::variant<std::string, int64_t, double>` as the value type for heterogeneous storage. Requires a type discriminator byte in `RadishEvent`. |
| **Opaque binary blobs** | Planned | Store raw bytes (`std::vector<std::byte>`) as a first-class value type. Radish treats the payload as an uninterpreted byte sequence — length-prefix on write, raw copy on read — and the calling service owns all interpretation of the binary format. Enables use-cases like storing serialised protobuf messages, MessagePack payloads, or any custom wire format without Radish needing to know the schema. The `HeapAllocated` concept already covers `std::vector<std::byte>` structurally; the main work is validating the full round-trip and documenting the contract clearly. |
| **Unit tests (GoogleTest)** | Planned | Round-trip tests: write events, re-open, verify state matches. Test TTL expiry, rename, clear. Isolate `RadishStore`, `PersistenceLog`, `LogReader`, `LogWriter`, and `BinaryFile` independently. Use mock `IClock` to test TTL without real time passing. |

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
│   ├── enums/
│   │   └── EventType.h         # EventType enum + TryGet* helpers (std::optional)
│   ├── helpers/
│   │   ├── Concepts.h          # BinaryType, HeapAllocated concepts + Serializable forward decl
│   │   ├── SystemClock.h       # IClock interface + SystemClock (chrono-backed)
│   │   └── Types.h             # Timestamp, BinarySize type aliases (zero dependencies)
│   ├── persistence/
│   │   ├── BinaryFile.h        # Static Read/Write helpers (arithmetic, HeapAllocated, Serializable)
│   │   ├── CompactStrategy.h   # Strategy pattern: CreateCompactStrategy, RenameCompactStrategy, etc.
│   │   ├── LogReader.h         # Binary file reader: deserialises events one at a time
│   │   ├── LogWriter.h         # Append / Replay / Compact; owns a LogReader internally
│   │   ├── PersistenceLog.h    # Thread-safe AOF facade: mutex + delegates to LogWriter
│   │   └── Serializable.h      # Pure-virtual base: Serialize + Deserialize
│   ├── RadishDB.h              # Public facade: composes RadishStore + PersistenceLog + shared_mutex
│   ├── RadishEvent.h           # Typed event: op + timestamp + key + payload
│   └── RadishStore.h           # Pure in-memory key-value store with TTL
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
