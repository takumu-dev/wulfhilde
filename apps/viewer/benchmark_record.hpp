#pragma once

#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace viewer {

/// How a measure spread over a benchmark's frames.
template <typename T> struct Spread {
    /// The 50th percentile, the median.
    T p50{};

    /// The 99th percentile.
    T p99{};

    /// The largest.
    T max{};
};

/// Returns how values spread: their 50th and 99th percentiles by nearest rank, each the least of
/// the values with at least that share of them at or below it, and their largest.
///
/// \param values The values, in any order, at least one.
/// \return Their spread.
template <typename T> [[nodiscard]] Spread<T> spread_of(std::span<const T> values) {
    WULFHILDE_ASSERT(!values.empty(), "a spread is of one value or more");
    std::vector<T> sorted(values.begin(), values.end());
    std::ranges::sort(sorted);
    // The nearest rank of a percentile p is p% of the count, rounded up, counting from 1.
    const auto at_percentile = [&sorted](std::size_t percent) {
        const auto rank = ((sorted.size() * percent) + 99) / 100;
        return sorted.at(std::max<std::size_t>(rank, 1) - 1);
    };
    return {.p50 = at_percentile(50), .p99 = at_percentile(99), .max = sorted.back()};
}

/// How long each stage of a benchmark's loading took, from opening the map's file to its first
/// frame drawn.
struct LoadTimes {
    /// Reading the map's file and parsing it.
    std::chrono::nanoseconds parse{};

    /// Making the map from what was parsed, checking its rules.
    std::chrono::nanoseconds map_create{};

    /// Loading the map's assets: its materials and their images.
    std::chrono::nanoseconds assets{};

    /// Uploading the map and its assets to the device.
    std::chrono::nanoseconds upload{};

    /// Drawing the first frame, from the start position.
    std::chrono::nanoseconds first_frame{};

    /// The whole of it, from opening the map's file to the end of the first frame.
    std::chrono::nanoseconds total{};
};

/// What one frame of a benchmark cost.
struct BenchmarkFrame {
    /// The CPU time of the frame's ticks, placing its eye, and recording and submitting its work.
    std::chrono::nanoseconds cpu{};

    /// What the renderer measured of the frame.
    wulfhilde::FrameMeasurement measurement{};
};

/// The record of a benchmark: what was measured, on what, and how.
struct BenchmarkRecord {
    /// The map's asset path.
    std::string map{};

    /// The benchmark walk's name.
    std::string walk{};

    /// The size each frame was drawn at.
    wulfhilde::Extent extent{};

    /// How many samples each pixel took.
    std::size_t samples = 0;

    /// How many times anisotropic filtering sampled a texture.
    std::size_t anisotropy = 0;

    /// The device's name.
    std::string device{};

    /// The device's driver and its version.
    std::string driver{};

    /// The operating system and its version.
    std::string os{};

    /// How the benchmark was run, in free text.
    std::string conditions{};

    /// How long loading took, by stage.
    LoadTimes load{};

    /// Each frame of the walk, in order, at least one.
    std::vector<BenchmarkFrame> frames{};
};

/// Writes a benchmark's record as one JSON object, ending in a line break.
///
/// Times are in milliseconds, to the microsecond. Each per-frame measure holds its spread, as
/// `p50`, `p99` and `max`, and then its value in every frame, as `frames`: the CPU time
/// (`cpu_ms`), the GPU time (`gpu_ms`) and the GPU time of each pass (`gpu_ms_by_pass`), the views,
/// the shadow maps rendered and the draws. Peak GPU memory is the most device memory the renderer
/// held after any frame, in bytes.
///
/// \param record The record, whose frames all have the same passes, in the same order.
/// \return The JSON.
[[nodiscard]] std::string format_record(const BenchmarkRecord& record);

} // namespace viewer
