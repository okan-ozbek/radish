#ifndef RADISH_BENCHMARK_STATISTICS_H
#define RADISH_BENCHMARK_STATISTICS_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

using BenchmarkNanoseconds = std::chrono::nanoseconds;

struct BenchmarkStatistics {
    std::uint64_t operations{};
    std::uint64_t errors{};
    double throughput{};
    double minimumMilliseconds{};
    double meanMilliseconds{};
    double p50Milliseconds{};
    double p99Milliseconds{};
    double p995Milliseconds{};
    double maximumMilliseconds{};
};

inline double BenchmarkMilliseconds(const BenchmarkNanoseconds value) {
    return std::chrono::duration<double, std::milli>(value).count();
}

inline BenchmarkNanoseconds BenchmarkPercentile(const std::vector<BenchmarkNanoseconds>& samples, const double percentile) {
    const auto rank = static_cast<std::size_t>(std::ceil(percentile * samples.size()));
    return samples[std::max<std::size_t>(rank, 1) - 1];
}

inline BenchmarkStatistics CalculateBenchmarkStatistics(
    std::vector<BenchmarkNanoseconds> samples,
    const std::uint64_t errors,
    const double durationSeconds
) {
    if (samples.empty() || durationSeconds <= 0) {
        throw std::invalid_argument("Benchmark statistics require samples and positive duration");
    }
    std::sort(samples.begin(), samples.end());

    BenchmarkNanoseconds total{};
    for (const auto sample : samples) {
        total += sample;
    }

    return {
        samples.size(),
        errors,
        static_cast<double>(samples.size()) / durationSeconds,
        BenchmarkMilliseconds(samples.front()),
        BenchmarkMilliseconds(total) / static_cast<double>(samples.size()),
        BenchmarkMilliseconds(BenchmarkPercentile(samples, 0.50)),
        BenchmarkMilliseconds(BenchmarkPercentile(samples, 0.99)),
        BenchmarkMilliseconds(BenchmarkPercentile(samples, 0.995)),
        BenchmarkMilliseconds(samples.back()),
    };
}

#endif //RADISH_BENCHMARK_STATISTICS_H
