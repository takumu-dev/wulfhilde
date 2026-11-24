// A benchmark's record: the spread of each measure over its frames, and the JSON it is written as.
#include "benchmark_record.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace {

// Times in whole microseconds and milliseconds.
std::chrono::microseconds us(std::int64_t count) { return std::chrono::microseconds{count}; }

std::chrono::milliseconds ms(std::int64_t count) { return std::chrono::milliseconds{count}; }

// A frame of the record below, with each pass's GPU time and the frame's counts.
viewer::BenchmarkFrame frame(std::chrono::nanoseconds cpu, std::chrono::nanoseconds main,
                             std::chrono::nanoseconds exposure, std::chrono::nanoseconds resolve,
                             std::size_t draws, std::size_t device_memory) {
    return {
        .cpu = cpu,
        .measurement =
            {
                .submission = ms(1),
                .gpu = main + exposure + resolve,
                .passes =
                    {
                        {.name = "main", .time = main},
                        {.name = "exposure", .time = exposure},
                        {.name = "resolve", .time = resolve},
                    },
                .views = 1,
                .shadow_maps = 0,
                .draws = draws,
                .device_memory = device_memory,
            },
    };
}

viewer::BenchmarkRecord two_frames() {
    return {
        .map = "sectors",
        .walk = "tour",
        .extent = {.width = 1920, .height = 1080},
        .samples = 4,
        .anisotropy = 16,
        .device = "llvmpipe (LLVM 20.1.2, 256 bits)",
        .driver = "llvmpipe Mesa 25.0.7",
        .os = "Linux 7.1.9 x86_64",
        .conditions = "on mains",
        .load =
            {
                .parse = us(1500),
                .map_create = us(250),
                .assets = ms(40),
                .upload = ms(12),
                .first_frame = ms(30),
                .total = ms(84),
            },
        .frames =
            {
                frame(ms(2), ms(8), us(1500), us(500), 3, 1000),
                frame(ms(3), us(9750), us(1750), us(500), 5, 2000),
            },
    };
}

} // namespace

TEST_CASE("a spread is the 50th and 99th percentiles by nearest rank, and the maximum",
          "[viewer]") {
    const std::vector<int> one{5};
    const auto alone = viewer::spread_of<int>(one);
    CHECK(alone.p50 == 5);
    CHECK(alone.p99 == 5);
    CHECK(alone.max == 5);

    const std::vector<int> unsorted{4, 1, 3, 2};
    const auto four = viewer::spread_of<int>(unsorted);
    CHECK(four.p50 == 2);
    CHECK(four.p99 == 4);
    CHECK(four.max == 4);

    std::vector<int> hundred(100);
    std::ranges::iota(hundred, 1);
    const auto spread = viewer::spread_of<int>(hundred);
    CHECK(spread.p50 == 50);
    CHECK(spread.p99 == 99);
    CHECK(spread.max == 100);
}

TEST_CASE("a benchmark's record is one JSON object, with each measure's spread and frames",
          "[viewer]") {
    const auto json = viewer::format_record(two_frames());

    CHECK(json == R"json({
  "map": "sectors",
  "walk": "tour",
  "width": 1920,
  "height": 1080,
  "samples": 4,
  "anisotropy": 16,
  "device": "llvmpipe (LLVM 20.1.2, 256 bits)",
  "driver": "llvmpipe Mesa 25.0.7",
  "os": "Linux 7.1.9 x86_64",
  "conditions": "on mains",
  "load_ms": {"parse": 1.500, "map_create": 0.250, "assets": 40.000, "upload": 12.000, "first_frame": 30.000, "total": 84.000},
  "peak_gpu_memory_bytes": 2000,
  "frame_count": 2,
  "cpu_ms": {"p50": 2.000, "p99": 3.000, "max": 3.000, "frames": [2.000, 3.000]},
  "gpu_ms": {"p50": 10.000, "p99": 12.000, "max": 12.000, "frames": [10.000, 12.000]},
  "gpu_ms_by_pass": {
    "main": {"p50": 8.000, "p99": 9.750, "max": 9.750, "frames": [8.000, 9.750]},
    "exposure": {"p50": 1.500, "p99": 1.750, "max": 1.750, "frames": [1.500, 1.750]},
    "resolve": {"p50": 0.500, "p99": 0.500, "max": 0.500, "frames": [0.500, 0.500]}
  },
  "views": {"p50": 1, "p99": 1, "max": 1, "frames": [1, 1]},
  "shadow_maps": {"p50": 0, "p99": 0, "max": 0, "frames": [0, 0]},
  "draws": {"p50": 3, "p99": 5, "max": 5, "frames": [3, 5]}
}
)json");
}

TEST_CASE("a benchmark's record escapes its free text as JSON strings", "[viewer]") {
    auto record = two_frames();
    record.conditions = "\"quoted\", back\\slash, line\nbreak, tab\tand \x01 ";

    const auto json = viewer::format_record(record);

    CHECK(json.contains(
        R"("conditions": "\"quoted\", back\\slash, line\nbreak, tab\tand \u0001 ",)"));
}
