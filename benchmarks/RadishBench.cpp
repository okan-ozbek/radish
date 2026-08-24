#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <sys/utsname.h>

#include "network/RespCodec.h"
#include "benchmark/BenchmarkStatistics.h"

namespace {
using Clock = std::chrono::steady_clock;
using Nanoseconds = BenchmarkNanoseconds;
using Tcp = asio::ip::tcp;

struct Options {
    std::string host{ "127.0.0.1" };
    std::string port{ "6379" };
    std::string workload{ "GET_HIT" };
    std::string outputPrefix{ "benchmark-result" };
    std::string label{ "unspecified" };
    std::size_t clients{ 1 };
    std::size_t pipeline{ 1 };
    std::size_t durationSeconds{ 60 };
    std::size_t warmupSeconds{ 30 };
    std::size_t keyCount{ 100000 };
    std::size_t keySize{ 32 };
    std::size_t valueSize{ 128 };
    std::uint64_t seed{ 1 };
    bool preload{ true };
};

struct WorkerResult {
    std::vector<Nanoseconds> latencies;
    std::uint64_t errors{};
};

Options ParseOptions(const int argumentCount, char* arguments[]) {
    Options options;
    for (int index = 1; index < argumentCount; index += 2) {
        if (index + 1 >= argumentCount) {
            throw std::invalid_argument("Missing value for " + std::string(arguments[index]));
        }
        const std::string option{ arguments[index] };
        const std::string value{ arguments[index + 1] };
        const auto number = [&value] {
            return static_cast<std::size_t>(std::stoull(value));
        };

        if (option == "--host") {
            options.host = value;
        } else if (option == "--port") {
            options.port = value;
        } else if (option == "--workload") {
            options.workload = value;
        } else if (option == "--clients") {
            options.clients = number();
        } else if (option == "--pipeline") {
            options.pipeline = number();
        } else if (option == "--duration") {
            options.durationSeconds = number();
        } else if (option == "--warmup") {
            options.warmupSeconds = number();
        } else if (option == "--key-count") {
            options.keyCount = number();
        } else if (option == "--key-size") {
            options.keySize = number();
        } else if (option == "--value-size") {
            options.valueSize = number();
        } else if (option == "--seed") {
            options.seed = std::stoull(value);
        } else if (option == "--output") {
            options.outputPrefix = value;
        } else if (option == "--label") {
            options.label = value;
        } else if (option == "--preload") {
            options.preload = value != "off";
        } else {
            throw std::invalid_argument("Unknown option: " + option);
        }
    }

    if (options.clients == 0 || options.pipeline == 0 || options.keyCount == 0 || options.keySize == 0) {
        throw std::invalid_argument("clients, pipeline, key-count, and key-size must be positive");
    }
    return options;
}

std::string MakeKey(const std::size_t index, const std::size_t size) {
    std::string key = "bench:" + std::to_string(index);
    if (key.size() >= size) {
        return key.substr(0, size);
    }
    key.append(size - key.size(), 'k');
    return key;
}

std::string MakeValue(const std::size_t size) {
    return std::string(size, 'v');
}

class RespConnection {
public:
    RespConnection(const std::string& host, const std::string& port)
        : m_resolver{ m_context }
        , m_socket{ m_context }
    {
        asio::connect(m_socket, m_resolver.resolve(host, port));
        m_socket.set_option(Tcp::no_delay(true));
    }

    bool Execute(const std::vector<std::string>& command) {
        const auto request = radish::network::RespCodec::EncodeCommand(command);
        asio::write(m_socket, asio::buffer(request));
        return ReadResponse();
    }

    void ExecutePipeline(
        const std::vector<std::vector<std::string>>& commands,
        std::vector<bool>& responses,
        std::vector<Clock::time_point>& completions
    ) {
        std::string request;
        for (const auto& command : commands) {
            request += radish::network::RespCodec::EncodeCommand(command);
        }
        asio::write(m_socket, asio::buffer(request));

        responses.clear();
        responses.reserve(commands.size());
        completions.clear();
        completions.reserve(commands.size());
        for (std::size_t index = 0; index < commands.size(); ++index) {
            responses.push_back(ReadResponse());
            completions.push_back(Clock::now());
        }
    }

private:
    asio::io_context m_context;
    Tcp::resolver m_resolver;
    Tcp::socket m_socket;
    asio::streambuf m_buffer;

    std::string ReadLine() {
        asio::read_until(m_socket, m_buffer, "\r\n");
        std::istream input(&m_buffer);
        std::string line;
        std::getline(input, line);
        line.pop_back();
        return line;
    }

    void ConsumeBytes(const std::size_t size) {
        while (m_buffer.size() < size) {
            asio::read(m_socket, m_buffer, asio::transfer_at_least(size - m_buffer.size()));
        }
        std::istream input(&m_buffer);
        input.ignore(static_cast<std::streamsize>(size));
    }

    bool ReadResponse(const std::size_t depth = 0) {
        if (depth > 32) {
            throw std::runtime_error("RESP response nesting limit exceeded");
        }
        const auto line = ReadLine();
        if (line.empty()) {
            throw std::runtime_error("Empty RESP response");
        }

        if (line.front() == '-' ) {
            return false;
        }
        if (line.front() == '+' || line.front() == ':' || line.front() == '_' || line.front() == '#') {
            return true;
        }
        if (line.front() == '$') {
            const auto length = std::stoll(line.substr(1));
            if (length >= 0) {
                ConsumeBytes(static_cast<std::size_t>(length) + 2);
            }
            return true;
        }
        if (line.front() == '*' || line.front() == '%') {
            const auto count = std::stoll(line.substr(1));
            if (count < 0) {
                return true;
            }
            const auto elements = line.front() == '%' ? count * 2 : count;
            bool success = true;
            for (long long index = 0; index < elements; ++index) {
                success = ReadResponse(depth + 1) && success;
            }
            return success;
        }

        throw std::runtime_error("Unsupported RESP response type");
    }
};

std::vector<std::string> BuildCommand(
    const Options& options,
    std::mt19937_64& generator,
    const std::uint64_t sequence
) {
    std::uniform_int_distribution<std::size_t> keyIndex(0, options.keyCount - 1);
    const auto key = MakeKey(keyIndex(generator), options.keySize);
    const auto workload = options.workload;

    if (workload == "PING") {
        return { "PING" };
    }
    if (workload == "SET") {
        return { "SET", key, MakeValue(options.valueSize) };
    }
    if (workload == "GET_HIT") {
        return { "GET", key };
    }
    if (workload == "GET_MISS") {
        return { "GET", MakeKey(options.keyCount + keyIndex(generator), options.keySize) };
    }
    if (workload == "DEL") {
        return { "DEL", key };
    }
    if (workload == "EXISTS") {
        return { "EXISTS", key };
    }
    if (workload == "DBSIZE") {
        return { "DBSIZE" };
    }
    if (workload == "KEYS") {
        return { "KEYS", "*" };
    }
    if (workload == "RENAME") {
        return { "RENAME", key, MakeKey(options.keyCount * 2 + sequence, options.keySize) };
    }
    if (workload == "MIX50" || workload == "READ95" || workload == "WRITE95") {
        const auto writePercent = workload == "MIX50" ? 50 : workload == "READ95" ? 5 : 95;
        std::uniform_int_distribution<int> percentage(1, 100);
        if (percentage(generator) <= writePercent) {
            return { "SET", key, MakeValue(options.valueSize) };
        }
        return { "GET", key };
    }

    throw std::invalid_argument("Unsupported workload: " + workload);
}

void Preload(const Options& options) {
    if (!options.preload) {
        return;
    }
    RespConnection connection(options.host, options.port);
    for (std::size_t index = 0; index < options.keyCount; ++index) {
        if (!connection.Execute({ "SET", MakeKey(index, options.keySize), MakeValue(options.valueSize) })) {
            throw std::runtime_error("Preload SET failed");
        }
    }
}

WorkerResult RunWorker(
    const Options& options,
    RespConnection& connection,
    const std::size_t workerId,
    const Clock::time_point warmupEnd,
    const Clock::time_point measurementEnd
) {
    std::mt19937_64 generator(options.seed + workerId);
    std::vector<std::vector<std::string>> commands;
    std::vector<bool> responses;
    std::vector<Clock::time_point> completions;
    std::uint64_t sequence{};

    const auto runUntil = [&](const Clock::time_point end, WorkerResult& result, const bool measure) {
        while (Clock::now() < end) {
            commands.clear();
            for (std::size_t index = 0; index < options.pipeline; ++index) {
                commands.push_back(BuildCommand(options, generator, sequence++));
            }

            const auto started = Clock::now();
            connection.ExecutePipeline(commands, responses, completions);
            if (!measure) {
                continue;
            }

            for (std::size_t index = 0; index < responses.size(); ++index) {
                if (responses[index]) {
                    result.latencies.push_back(std::chrono::duration_cast<Nanoseconds>(completions[index] - started));
                } else {
                    ++result.errors;
                }
            }
        }
    };

    WorkerResult result;
    runUntil(warmupEnd, result, false);
    runUntil(measurementEnd, result, true);
    return result;
}

std::string SystemMetadata() {
    utsname information{};
    if (uname(&information) != 0) {
        return "unknown";
    }
    return std::string(information.sysname) + " " + information.release + " " + information.machine;
}

void WriteResults(const Options& options, const BenchmarkStatistics& statistics) {
    const auto jsonPath = options.outputPrefix + ".json";
    const auto csvPath = options.outputPrefix + ".csv";
    std::ofstream json(jsonPath);
    std::ofstream csv(csvPath);
    if (!json || !csv) {
        throw std::runtime_error("Failed to create benchmark result files");
    }

    json << "{\n"
         << "  \"host\": \"" << options.host << "\",\n"
         << "  \"port\": \"" << options.port << "\",\n"
         << "  \"label\": \"" << options.label << "\",\n"
         << "  \"workload\": \"" << options.workload << "\",\n"
         << "  \"clients\": " << options.clients << ",\n"
         << "  \"pipeline\": " << options.pipeline << ",\n"
         << "  \"duration_seconds\": " << options.durationSeconds << ",\n"
         << "  \"warmup_seconds\": " << options.warmupSeconds << ",\n"
         << "  \"key_count\": " << options.keyCount << ",\n"
         << "  \"key_size\": " << options.keySize << ",\n"
         << "  \"value_size\": " << options.valueSize << ",\n"
         << "  \"seed\": " << options.seed << ",\n"
         << "  \"environment\": \"" << SystemMetadata() << "\",\n"
         << "  \"operations\": " << statistics.operations << ",\n"
         << "  \"errors\": " << statistics.errors << ",\n"
         << "  \"throughput_ops_per_second\": " << statistics.throughput << ",\n"
         << "  \"latency_ms\": { \"min\": " << statistics.minimumMilliseconds
         << ", \"mean\": " << statistics.meanMilliseconds
         << ", \"p50\": " << statistics.p50Milliseconds
         << ", \"p99\": " << statistics.p99Milliseconds
         << ", \"p99_5\": " << statistics.p995Milliseconds
         << ", \"max\": " << statistics.maximumMilliseconds << " }\n"
         << "}\n";

    csv << "workload,clients,pipeline,operations,errors,throughput_ops_per_second,min_ms,mean_ms,p50_ms,p99_ms,p99_5_ms,max_ms\n"
        << options.workload << ',' << options.clients << ',' << options.pipeline << ',' << statistics.operations << ','
        << statistics.errors << ',' << statistics.throughput << ',' << statistics.minimumMilliseconds << ','
        << statistics.meanMilliseconds << ',' << statistics.p50Milliseconds << ',' << statistics.p99Milliseconds << ','
        << statistics.p995Milliseconds << ',' << statistics.maximumMilliseconds << '\n';
}
}

int main(const int argumentCount, char* arguments[]) {
    try {
        if (argumentCount == 2 && std::string_view(arguments[1]) == "--help") {
            std::cout << "Usage: radish-bench [--host HOST] [--port PORT] [--workload NAME] [--clients N]"
                         " [--pipeline N] [--duration SECONDS] [--warmup SECONDS] [--key-count N]"
                         " [--key-size BYTES] [--value-size BYTES] [--seed N] [--output PREFIX]"
                         " [--label NAME] [--preload off]\n";
            return 0;
        }
        const auto options = ParseOptions(argumentCount, arguments);
        Preload(options);

        std::vector<std::unique_ptr<RespConnection>> connections;
        connections.reserve(options.clients);
        for (std::size_t index = 0; index < options.clients; ++index) {
            connections.push_back(std::make_unique<RespConnection>(options.host, options.port));
        }

        const auto warmupEnd = Clock::now() + std::chrono::seconds(options.warmupSeconds);
        const auto measurementEnd = warmupEnd + std::chrono::seconds(options.durationSeconds);
        std::vector<WorkerResult> results(options.clients);
        std::vector<std::exception_ptr> failures(options.clients);
        std::vector<std::thread> workers;
        workers.reserve(options.clients);
        for (std::size_t index = 0; index < options.clients; ++index) {
            workers.emplace_back([&options, &connections, &failures, &results, index, warmupEnd, measurementEnd] {
                try {
                    results[index] = RunWorker(
                        options,
                        *connections[index],
                        index,
                        warmupEnd,
                        measurementEnd
                    );
                }
                catch (...) {
                    failures[index] = std::current_exception();
                }
            });
        }

        for (auto& worker : workers) {
            worker.join();
        }
        for (const auto& failure : failures) {
            if (failure) {
                std::rethrow_exception(failure);
            }
        }

        std::vector<Nanoseconds> samples;
        std::uint64_t errors{};
        for (auto& result : results) {
            errors += result.errors;
            samples.insert(samples.end(), result.latencies.begin(), result.latencies.end());
        }

        const auto statistics = CalculateBenchmarkStatistics(
            std::move(samples),
            errors,
            static_cast<double>(options.durationSeconds)
        );
        WriteResults(options, statistics);
        std::cout << "workload=" << options.workload
                  << " throughput_ops_per_second=" << statistics.throughput
                  << " min_ms=" << statistics.minimumMilliseconds
                  << " p50_ms=" << statistics.p50Milliseconds
                  << " p99_ms=" << statistics.p99Milliseconds
                  << " p99_5_ms=" << statistics.p995Milliseconds
                  << " max_ms=" << statistics.maximumMilliseconds
                  << " errors=" << statistics.errors << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "radish-bench: " << error.what() << '\n';
        return 1;
    }
}
