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
RadishDB<TValue>              -- Public facade: composes data + persistence
    Radish<TValue>            -- Pure in-memory key-value store (no I/O)
    Recorder<TValue>          -- AOF write layer (appends to .rdh file)
    Replayer<TValue>          -- AOF replay on startup (reads .rdh file)
        Operation.h           -- Shared OperationType enum + name helpers
```

This means `Radish` can be instantiated and tested in complete isolation from any file system. The persistence
layer is entirely opt-in through `RadishDB`.

---

## Current Implementation

### Key-Value Store — `Radish<TValue>`

A generic in-memory store backed by `std::unordered_map<std::string, TValue>`. Accepts any value type
that supports `operator<<` for serialisation into the AOF log.

| Method | Redis Equivalent | Description |
|---|---|---|
| `Insert(key, value)` | `SET key value` | Insert or overwrite a value |
| `Fetch(key)` | `GET key` | Returns `std::optional<TValue>`, nullopt if missing |
| `Remove(key)` | `DEL key` | Delete a key |
| `Clear()` | `FLUSHDB` | Delete all keys |
| `Exists(key)` | `EXISTS key` | Check if a key is present |

### Persistence — Append-Only File (AOF)

Every write operation is durably logged to a `.rdh` file. On startup, the log is replayed line by line to
reconstruct the previous state in memory. This is exactly how Redis's AOF persistence mode works.

The log format is intentionally human-readable:

```
INSERT users:id:1 Alice
INSERT users:id:2 Bob
REMOVE users:id:1
INSERT users:id:1 Charlie
```

- **`Recorder<TValue>`** — opens the file in append mode on construction (RAII), writes one line per operation.
- **`Replayer<TValue>`** — reads the file on startup using `std::getline` + `std::istringstream` and replays
  each command against a `Radish` instance.

### Public Interface — `RadishDB<TValue>`

The facade that ties everything together. Using `RadishDB` gives you a fully persistent store with the same
interface as `Radish`.

```cpp
RadishDB<std::string> db("my_database");

db.Insert("users:id:1", "Alice");
db.Insert("users:id:2", "Bob");
db.Remove("users:id:1");

// On next run: state is automatically restored from my_database.rdh
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| **Templates over inheritance** | Allows the store to hold any type without virtual dispatch or type erasure overhead |
| **`std::optional` for Fetch** | Avoids sentinel values (`""`, `-1`) and exceptions for the normal "key not found" case |
| **RAII for file management** | `Recorder` opens on construction, closes in its destructor — no manual cleanup needed |
| **Persistence outside `Radish`** | `Radish` is a pure data structure. Persistence is an infrastructure concern, not a data concern |
| **One operation per line in AOF** | Simple to write, simple to parse, human-readable, and easy to compact later |
| **`std::istringstream` for parsing** | Parses each log line in one pass without a state machine — cleaner than token-by-token `>>` across iterations |
| **`enum` in `Operation.h`** | `OperationType` is shared by `Recorder` and `Replayer` — belongs in neither, so lives in a shared header |

---

## Roadmap

This section describes what it would take to turn Radish from a learning project into something genuinely
impressive and rigorous as a C++ systems project.

### Short Term — Completeness

These are small, self-contained additions that bring Radish to feature parity with Redis's most basic commands.

| Feature | Description |
|---|---|
| **Key TTL / Expiry** | Attach an `std::chrono` expiry timestamp to each entry. `Fetch` returns `nullopt` for expired keys. Mirrors `EXPIRE` / `TTL`. |
| **`Keys()` / `Scan()`** | Return all live keys. Useful for debugging and introspection. |
| **`Rename(old, new)`** | Atomic key rename without a round-trip Remove + Insert. |
| **`Size()` / `IsEmpty()`** | Aggregate queries on the store. |
| **Error handling** | Replace bare `throw` with a proper result type or error code to avoid exceptions in hot paths. |

### Medium Term — Robustness

These additions make Radish more production-like and force engagement with harder C++ and systems concepts.

| Feature | Description |
|---|---|
| **AOF Compaction** | Rewrite the log to its minimal equivalent: eliminate superseded `INSERT` calls, remove keys that were later `REMOVE`'d. Prevents unbounded log growth. |
| **Snapshot persistence (RDB-like)** | Periodically serialise the entire in-memory store to a compact file. Faster to restore than replaying a long AOF log. Run both together like Redis does: AOF for durability, snapshot for fast startup. |
| **`std::variant` value types** | Store `std::variant<std::string, int64_t, double>` as the value type to support heterogeneous data under one store instance — closer to real Redis. |
| **Unit tests (GoogleTest)** | Cover `Radish`, `Recorder`, and `Replayer` in isolation. AOF round-trip tests: write operations, restart, verify state is identical. |
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
│   ├── Operation.h         # OperationType enum + name/type conversion helpers
│   ├── Radish.h            # Pure in-memory key-value store
│   ├── Recorder.h          # AOF write layer
│   ├── Replayer.h          # AOF replay on startup
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
