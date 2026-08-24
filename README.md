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
            BinaryFile            -- Static R/W helpers (arithmetic and byte containers)
        CompactStrategy<TValue>   -- Strategy pattern: per-operation compaction logic
        enums/EventType.h         -- EventType enum (uint8_t) + TryGet* helpers
        helpers/Types.h           -- Timestamp, BinarySize type aliases
        helpers/Concepts.h        -- BinaryType and HeapAllocated concepts
        helpers/SystemClock.h     -- IClock interface + SystemClock (chrono-backed)
```

`RadishStore` can be instantiated and tested with zero file system involvement. The persistence layer is
entirely opt-in through `RadishDB`. The AOF is the durable source of truth; `RadishStore` is its in-memory
materialized view. `PersistenceLog` is a thin thread-safe facade over `LogWriter`, which owns `Append`,
`Replay`, and explicit `Compact`. `LogReader` is kept as a separate, single-responsibility class so
that read and write paths can evolve independently — `RadishDB` calls `m_persistence.Replay(m_store)` and
nothing more.

A write is acknowledged after its framed record has been appended and the C++ stream successfully flushed.
This protects the in-memory view from append failures, but it is not an OS-level power-loss durability guarantee.

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

Every state-changing operation is serialised as a `RadishEvent` and appended to a `.radish` binary file before
the in-memory state is committed. On startup, `PersistenceLog::Replay` reads all events and applies them in
order to reconstruct state. `PersistenceLog::Compact` is explicit, not a destructor side effect.

The binary format is operation-driven — only fields relevant to the operation are written:

| Operation | Fields written |
|---|---|
| `CREATE` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `payload` |
| `DELETE` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` |
| `RENAME` | `opType (1B)` + `timestamp (8B)` + `keyLen (4B)` + `key` + `renameKeyLen (4B)` + `renameKey` |
| `CLEAR` | `opType (1B)` + `timestamp (8B)` |

Each file starts with a format header. Each event is length-framed before its operation-driven payload, so readers
can reject invalid or oversized records and ignore an incomplete final write left by a crash.

The persistence layer is split into three focused classes:

- **`PersistenceLog<TValue>`** — thin, thread-safe facade. Owns a `std::mutex`, forwards `Append`,
  `Replay`, and explicit `Compact` to `LogWriter`.
- **`LogWriter<TValue>`** — owns the write path entirely: `Append` (single event append), `Replay` (apply
  all events to a store), and `Compact` (reduce the log to its minimal equivalent). Internally holds a
  `LogReader` to read back events during replay and compaction.
- **`LogReader<TValue>`** — owns the read path: opens the file in binary mode and deserialises framed events
  one at a time. An incomplete final record is ignored; invalid complete records are rejected.
- **`RadishEvent<TValue>`** — typed event object that serialises/deserialises itself using `BinaryFile`
  helpers with compile-time `if constexpr` dispatch per field.

### AOF Compaction — `CompactStrategy<TValue>`

When explicitly invoked, `PersistenceLog` reduces the log to its minimal equivalent using the Strategy pattern. Each
`EventType` has a dedicated strategy that knows how to apply that operation to a live key map:

| Strategy | Behaviour |
|---|---|
| `CreateCompactStrategy` | Inserts the event into the map under its key (latest `CREATE` wins) |
| `RenameCompactStrategy` | Moves the existing entry to the new key name |
| `DeleteCompactStrategy` | Erases the key from the map |
| `ClearCompactStrategy` | Clears the entire map |

After all strategies have been applied, `RewriteHistory` writes only the surviving `CREATE` events to a temporary
file, then atomically replaces the old file — skipping any whose TTL has already expired. The result is a file that contains exactly the
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

### Clock Abstraction — `helpers/SystemClock.h`

TTL expiry checks go through an `IClock` interface:

```cpp
struct IClock {
    [[nodiscard]] virtual Timestamp Now() const = 0;
};

struct SystemClock final : IClock {
    [[nodiscard]] Timestamp Now() const override; // std::chrono::system_clock
};
```

`RadishStore` currently holds a `SystemClock` by value.

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

---

## Design Decisions

| Decision | Rationale |
|---|---|
| **Templates over inheritance** | Allows the store to hold any type without virtual dispatch or type erasure overhead |
| **`std::optional` for `Get`** | Avoids sentinel values and exceptions for the normal "key not found" case |
| **`std::optional` for error handling** | `TryGet*` helpers return `std::nullopt` on unknown input — no exceptions in hot paths |
| **Binary format over text AOF** | Versioned headers and length-framed records keep the AOF compact while making malformed records detectable |
| **Operation-driven serialisation** | Only fields relevant to the operation are written — `CLEAR` writes 9 bytes total, not a padded record |
| **`RadishEvent` as typed event model** | Each event carries its own type, timestamp, key, and payload — self-describing and easy to replay |
| **`BinaryFile` as centralised I/O** | One place for checked `if constexpr` dispatch over arithmetic and trivially-copyable byte containers |
| **`HeapAllocated` concept** | Compile-time detection of types with `size()` + `data()` — covers `std::string`, `std::vector<T>`, and any compatible type. `sizeof(ElementType)` ensures correct byte count for non-char element types. `static_assert` on `is_trivially_copyable` prevents silent corruption. |
| **`Concepts.h` separate from `Types.h`** | `Types.h` owns shared persistence aliases; `Concepts.h` owns the supported value-type constraints. |
| **Strategy pattern for compaction** | Each `EventType` gets an isolated strategy class. Adding a new operation requires only a new strategy — `PersistenceLog` and `Compact()` need no changes. |
| **Explicit atomic compaction** | `Compact()` writes a flushed temporary AOF and atomically replaces the old file, avoiding destructor-time I/O and in-place truncation. |
| **`IClock` interface on `SystemClock`** | The clock contract keeps time access isolated for a future injectable store clock. |
| **`PersistenceLog::Replay` owns replay** | `RadishDB` calls one method. The switch over operation types lives in `PersistenceLog`, not in the facade |
| **`PersistenceLog` as a thin facade** | `PersistenceLog` owns only a mutex and delegates everything to `LogWriter`. Thread-safety concerns are isolated in one place and do not bleed into read or write logic. |
| **`LogReader` / `LogWriter` separation** | Read and write paths are independent classes. `LogWriter` can evolve its write strategy or compaction logic without touching the deserialisation code in `LogReader`, and vice versa. |
| **`std::shared_mutex` on `RadishDB`** | Concurrent reads (`Get`, `Scan`, `Exists`, etc.) use `shared_lock` so they run in parallel. Writes (`Create`, `Delete`, etc.) use exclusive `lock_guard`. Two independent mutexes — one in `RadishDB`, one in `PersistenceLog` — keep store and I/O state protected separately. |
| **Timestamp stored in event** | Expiry survives a restart: the absolute `Timestamp` is written so `Create(key, value, ts)` can restore it exactly |
| **Lazy expiry** | Expired keys are checked at access time — no background thread, no complexity |
| **`Rename` as atomic operation** | Implemented directly in `m_data` without a public `Delete` + `Create` round-trip |

---

## Roadmap

### Short Term — Completeness

All short-term features are complete.

| Feature | Status | Notes |
|---|---|---|
| **Key TTL / Expiry** | Done | Absolute timestamp stored per key. `Get`, `Exists`, `Scan` all respect TTL. Timestamp written to binary log and restored on replay. |
| **`Scan()`** | Done | Returns all live keys. Mirrors `KEYS *`. |
| **`Rename(old, new)`** | Done | Atomic in `m_data`. Recorded as `RENAME` event in binary log. |
| **`Size()`** | Done | Counts live keys, matching `Scan()` and `Exists()`. |
| **Error handling** | Done | Writes flush before acknowledgement; invalid complete records are rejected and incomplete final records are ignored. |

### Medium Term — Robustness

| Feature | Status | Description |
|---|---|---|
| **Binary serialisation** | Done | Versioned header, bounded length-framed records, checked reads/writes, and event-tag validation protect replay. |
| **AOF Compaction** | Done | Strategy pattern per `EventType`. Explicit `Compact()` writes a temporary compacted AOF then atomically replaces the old file. |
| **LogReader / LogWriter split** | Done | Read and write paths are now separate classes. `LogWriter` owns `Append`, `Replay`, and `Compact`; `LogReader` owns event deserialisation from disk. `PersistenceLog` is a thin mutex-guarded facade over `LogWriter`. |
| **Thread safety** | Done | `RadishDB` uses `std::shared_mutex`: `shared_lock` for concurrent reads (`Get`, `Scan`, `Size`, `Exists`, `IsExpired`), exclusive `lock_guard` for writes. `PersistenceLog` adds a second `std::mutex` guarding file I/O, so the two layers protect their own state independently. |
| **`std::variant` value types** | Planned | `std::variant<std::string, int64_t, double>` as the value type for heterogeneous storage. Requires a type discriminator byte in `RadishEvent`. |
| **Opaque binary blobs** | Done | `std::vector<std::byte>` is supported as a length-prefixed, trivially-copyable byte container. |
| **Unit tests** | Done | CTest regression coverage exercises replay of all operations, restart before compaction, compaction rename behavior, and torn final records. |

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
│   │   ├── Concepts.h          # BinaryType and HeapAllocated concepts
│   │   ├── SystemClock.h       # IClock interface + SystemClock (chrono-backed)
│   │   └── Types.h             # Timestamp, BinarySize type aliases (zero dependencies)
│   ├── persistence/
│   │   ├── BinaryFile.h        # Checked Read/Write helpers
│   │   ├── CompactStrategy.h   # Strategy pattern: CreateCompactStrategy, RenameCompactStrategy, etc.
│   │   ├── LogReader.h         # Binary file reader: deserialises events one at a time
│   │   ├── LogWriter.h         # Append / Replay / Compact; owns a LogReader internally
│   │   ├── PersistenceLog.h    # Thread-safe AOF facade: mutex + delegates to LogWriter
│   ├── RadishDB.h              # Public facade: composes RadishStore + PersistenceLog + shared_mutex
│   ├── RadishEvent.h           # Typed event: op + timestamp + key + payload
│   └── RadishStore.h           # Pure in-memory key-value store with TTL
├── src/
│   └── main.cpp                # Entry point
├── tests/
│   ├── PersistenceLogTests.cpp # AOF read/write, compaction, and failure-path coverage
│   ├── RadishDBTests.cpp       # Public API, restart, TTL, and compaction coverage
│   ├── RadishEventTests.cpp    # Event serialization and validation coverage
│   ├── RadishStoreTests.cpp    # In-memory mutation and TTL coverage
│   └── TestSupport.h           # Temporary database-file fixture
└── README.md
```

---

## Building

Requires CMake 3.25+ and a C++23 compiler.

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
./cmake-build-debug/radish
```

With a configured `cmake-build-debug` directory, the Bash wrappers mirror the orderbook workflow:

```bash
./scripts/app-build.sh
./scripts/app-test.sh
./scripts/app-test-verbose.sh
```

Or use the Makefile for an npm-script-like workflow:

```bash
make build
make test
make test-verbose
```

---

## Built With

- **Language:** C++23
- **Build System:** CMake 3.25+
- **IDE:** CLion
- **Compiler:** MinGW (GCC)

---

## License

Copyright &copy; 2026 Okan Ozbek. All rights reserved.
