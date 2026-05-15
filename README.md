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

```cpp
RadishDB<TValue>              -- Public facade: composes data + persistence
    Radish<TValue>            -- Pure in-memory key-value store (no I/O)
    Recorder                  -- AOF write layer (appends to .rdh file)
    Replayer<TValue>          -- AOF replay on startup (reads .rdh file)
        helpers/Operation.h   -- Shared OperationType enum + name helpers
        helpers/Types.h       -- Shared type aliases (MsTimestamp, MsType)
        helpers/SystemClock.h -- Clock abstraction for TTL expiry checks
```

This means `Radish` can be instantiated and tested in complete isolation from any file system. The persistence
layer is entirely opt-in through `RadishDB`.

---

## Current Implementation

### Key-Value Store — `Radish<TValue>`

A generic in-memory store backed by `std::unordered_map<std::string, TValue>`. Accepts any value type
that supports `operator<<` for serialisation into the AOF log.

TTL is opt-in: construct with a millisecond duration to enable expiry, or omit it for a persistent-forever store.
Expired keys are checked lazily on access — no background thread required.

| Method | Redis Equivalent | Description |
|---|---|---|
| `Set(key, value)` | `SET key value` | Insert or overwrite a value. Returns the expiry timestamp, or `-1` if TTL is disabled |
| `SetByTimestamp(key, value, timestamp)` | — | Internal: restores a key with a known expiry timestamp during AOF replay |
| `Get(key)` | `GET key` | Returns `std::optional<TValue>`. Returns `nullopt` if missing or expired |
| `Delete(key)` | `DEL key` | Delete a key |
| `Wipe()` | `FLUSHDB` | Delete all keys |
| `Rename(old, new)` | `RENAME old new` | Atomically move a value to a new key. No-op if old key does not exist |
| `Exists(key)` | `EXISTS key` | Returns `false` if the key is missing or expired |
| `Scan()` | `KEYS *` | Returns all live (non-expired) keys as `std::vector<std::string>` |
| `Size()` | `DBSIZE` | Returns total number of stored keys (including expired, not yet evicted) |
| `IsExpired(key)` | `TTL key` (partial) | Returns `true` if the key has a TTL and it has passed |
| `IsTTLEnabled()` | — | Returns `true` if the store was constructed with a TTL duration |
| `GetTTL()` | `TTL key` (partial) | Returns the configured TTL duration in milliseconds |

### Persistence — Append-Only File (AOF)

Every write operation is durably logged to a `.rdh` file. On startup, the log is replayed line by line to
reconstruct the previous state in memory. This mirrors how Redis's AOF persistence mode works.

The log format is intentionally human-readable:

```
SET users:id:1 1747353600000 Alice
SET users:id:2 1747353601000 Bob
DELETE users:id:1
SET users:id:1 1747353602000 Charlie
RENAME users:id:2 users:id:99
WIPE
```

Each line is one operation: `OPERATION [key] [timestamp] [value]`. The timestamp is stored so that expiry
is preserved correctly across restarts — not reset.

- **`Recorder`** — opens the file in append mode on construction (RAII). Exposes a variadic `TryAppend`
  method that serialises any operation + arguments to one line. Returns silently on unknown operations
  instead of throwing.
- **`Replayer<TValue>`** — reads the file on startup using `std::getline` + `std::istringstream` and
  replays each command against a `Radish` instance. Unknown operation names are skipped gracefully.

### Operations — `helpers/Operation.h`

Defines the `OperationType` enum shared by `Recorder` and `Replayer`, and two free functions for
converting between names and types — both returning `std::optional` instead of throwing:

```
SET     -- Write a key/value with an expiry timestamp
DELETE  -- Remove a key
WIPE    -- Clear the entire store
RENAME  -- Move a value to a new key
```

### Public Interface — `RadishDB<TValue>`

The facade that ties everything together. Using `RadishDB` gives you a fully persistent store with the same
interface as `Radish`.

```cpp
// No TTL — keys live forever
RadishDB<std::string> db("my_database");

db.Set("users:id:1", "Alice");
db.Set("users:id:2", "Bob");
db.Delete("users:id:1");
db.Rename("users:id:2", "users:id:99");

auto keys = db.Scan();     // { "users:id:99" }
auto size = db.Size();     // 1

// On next run: state is automatically restored from my_database.rdh
```

```cpp
// With TTL — keys expire after 5000ms
RadishDB<std::string> db("my_database", 5000);

db.Set("session:abc", "user_42");  // expires in 5 seconds
db.Exists("session:abc");          // true (within TTL)
// ... 5 seconds later ...
db.Exists("session:abc");          // false (expired)
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| **Templates over inheritance** | Allows the store to hold any type without virtual dispatch or type erasure overhead |
| **`std::optional` for `Get`** | Avoids sentinel values (`""`, `-1`) and exceptions for the normal "key not found" case |
| **`std::optional` for error handling** | `TryGetOperationTypeByName` and `TryGetNameByOperationType` return `std::nullopt` on unknown input instead of throwing — no exceptions in hot paths |
| **RAII for file management** | `Recorder` opens on construction, closes in its destructor — no manual cleanup needed |
| **Persistence outside `Radish`** | `Radish` is a pure data structure. Persistence is an infrastructure concern, not a data concern |
| **One operation per line in AOF** | Simple to write, simple to parse, human-readable, and easy to compact later |
| **Timestamp stored in AOF** | Expiry is serialised as an absolute millisecond timestamp so TTL survives a restart correctly |
| **`std::istringstream` for parsing** | Parses each log line in one pass without a state machine — cleaner than token-by-token `>>` across iterations |
| **Lazy expiry** | Expired keys are checked at access time instead of by a background thread — simpler and sufficient for a single-threaded store |
| **`enum` in `helpers/Operation.h`** | `OperationType` is shared by `Recorder` and `Replayer` — belongs in neither, so lives in a shared helpers header |
| **Variadic `TryAppend` in `Recorder`** | One private implementation handles all write shapes — no repeated lookup/error-handling logic across overloads |
| **`Rename` as atomic operation** | Implemented directly in `m_data` without calling `Delete` + `Set` publicly, avoiding a window where neither key exists |

---

## Roadmap

This section describes what it would take to turn Radish from a learning project into something genuinely
impressive and rigorous as a C++ systems project.

### Short Term — Completeness

All short-term features are implemented.

| Feature | Status | Description |
|---|---|---|
| **Key TTL / Expiry** | Done | `MsTimestamp` expiry stored per key. `Get`, `Exists`, `Scan` all respect TTL. Expiry timestamp is serialised to AOF and restored on replay. |
| **`Scan()`** | Done | Returns all live (non-expired) keys as `std::vector<std::string>`. Mirrors `KEYS *`. |
| **`Rename(old, new)`** | Done | Atomic move in `m_data` — no public `Delete` + `Set` round-trip. Recorded as a `RENAME` entry in AOF. |
| **`Size()`** | Done | Returns total number of stored keys via `m_data.size()`. |
| **Error handling** | Done | `TryGetOperationTypeByName` and `TryGetNameByOperationType` return `std::optional` — no exceptions in hot paths. Unknown log lines are skipped silently. |

### Medium Term — Robustness

These additions make Radish more production-like and force engagement with harder C++ and systems concepts.

| Feature | Description |
|---|---|
| **Binary serialisation** | Replace the human-readable text format with a compact binary encoding. A dedicated `Serializer` class encodes each operation and its payload to raw bytes. The `.rdh` file becomes a binary stream read manually with `std::ifstream` in binary mode (`std::ios::binary`). Fields are written as fixed-width integers and length-prefixed byte sequences — no delimiters, no parsing ambiguity. Faster to write, faster to replay, and smaller on disk. |
| **AOF Compaction** | Rewrite the log to its minimal equivalent: eliminate superseded `SET` calls, remove keys that were later `DELETE`'d. Prevents unbounded log growth. Compaction becomes trivial once a `Serializer` owns the encoding format. |
| **Snapshot persistence (RDB-like)** | Periodically serialise the entire in-memory store to a compact binary file. Faster to restore than replaying a long AOF log. Run both together like Redis: AOF for durability, snapshot for fast startup. |
| **`std::variant` value types** | Store `std::variant<std::string, int64_t, double>` as the value type to support heterogeneous data under one store instance — closer to real Redis. Requires the `Serializer` to encode a type discriminator byte before each value. |
| **Unit tests (GoogleTest)** | Cover `Radish`, `Recorder`, and `Replayer` in isolation. AOF round-trip tests: write operations, restart, verify state is identical. Binary round-trip tests once the `Serializer` is in place. |
| **Thread safety** | Wrap `m_data` access in a `std::shared_mutex` (readers-writer lock): concurrent reads, exclusive writes. A prerequisite for any networking work. |

### Long Term — From Toy to System

These are the features that elevate Radish from a library into a working piece of infrastructure software.

| Feature | Description |
|---|---|
| **TCP Server** | Wrap `RadishDB` in a TCP server using standalone Asio (no Boost). Accept connections and parse commands from clients — making Radish something you can actually `telnet` or connect to with a real client. |
| **RESP Protocol** | Implement the Redis Serialisation Protocol (RESP) so that real Redis clients (`redis-cli`, client libraries) can talk to Radish without modification. |
| **List type** | Redis-style list values: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`. Backed by `std::deque`. |
| **Set type** | Redis-style set values: `SADD`, `SREM`, `SMEMBERS`, `SINTER`. Backed by `std::unordered_set`. |
| **Pub/Sub** | A basic publish/subscribe system: clients subscribe to channels, other clients publish messages. Requires the networking layer first. |
| **Benchmarking** | Measure throughput (operations/sec) and latency (p50/p99) against real Redis on equivalent workloads. Make the numbers visible and quantifiable. |

---

## Project Structure

```
radish/
├── include/
│   ├── helpers/
│   │   ├── Operation.h     # OperationType enum + TryGet* helpers (returns std::optional)
│   │   ├── SystemClock.h   # Clock abstraction used for TTL expiry checks
│   │   └── Types.h         # Shared type aliases: MsTimestamp, MsType
│   ├── Radish.h            # Pure in-memory key-value store with TTL support
│   ├── Recorder.h          # AOF write layer (variadic TryAppend, append-mode RAII)
│   ├── Replayer.h          # AOF replay on startup (restores state from .rdh file)
│   └── RadishDB.h          # Public facade: composes Radish + Recorder + Replayer
├── src/
│   └── main.cpp            # Entry point
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
