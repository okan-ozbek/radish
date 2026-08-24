#include <chrono>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "benchmark/BenchmarkStatistics.h"

TEST_CASE("Benchmark statistics calculate exact nearest-rank percentiles", "[benchmark][statistics]")
{
    const std::vector<BenchmarkNanoseconds> samples{
        std::chrono::milliseconds(4),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(3),
        std::chrono::milliseconds(2),
    };

    const auto statistics = CalculateBenchmarkStatistics(samples, 2, 2.0);

    REQUIRE(statistics.operations == 4);
    REQUIRE(statistics.errors == 2);
    REQUIRE(statistics.throughput == 2.0);
    REQUIRE(statistics.minimumMilliseconds == 1.0);
    REQUIRE(statistics.meanMilliseconds == 2.5);
    REQUIRE(statistics.p50Milliseconds == 2.0);
    REQUIRE(statistics.p99Milliseconds == 4.0);
    REQUIRE(statistics.p995Milliseconds == 4.0);
    REQUIRE(statistics.maximumMilliseconds == 4.0);
}

TEST_CASE("Benchmark statistics reject empty samples and non-positive durations", "[benchmark][statistics]")
{
    REQUIRE_THROWS(CalculateBenchmarkStatistics({}, 0, 1.0));
    REQUIRE_THROWS(CalculateBenchmarkStatistics({ std::chrono::milliseconds(1) }, 0, 0.0));
}
