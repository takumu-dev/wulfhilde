#pragma once

#include "command_line.hpp"
#include "controller.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/world/map.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace viewer {

/// One timed step of a benchmark walk: actions held, and a turn spread evenly, over a number of
/// the controller's ticks.
struct BenchmarkStep {
    /// What the player asks for, each tick of the step.
    Actions actions{};

    /// How far the player turns left over the whole step.
    wulfhilde::Angle turn{};

    /// How many ticks the step lasts.
    std::size_t ticks = 0;
};

/// A map's timed walk for `--benchmark`, which plays the same frames every time, so that two
/// records of it compare frame for frame.
struct BenchmarkWalk {
    /// The asset path of the map the walk is for, in the test content.
    std::string_view map{};

    /// The walk's name, which `--benchmark` takes.
    std::string_view name{};

    /// The walk's steps, from the map's start position.
    std::span<const BenchmarkStep> steps{};
};

/// How many of the controller's ticks a benchmark plays for each frame it draws: 4, for 60 frames
/// a second.
inline constexpr std::size_t ticks_per_frame = 4;

/// Finds a benchmark walk by its map and its name.
///
/// \param map The map's asset path.
/// \param name The walk's name.
/// \return The walk, or the usage error of a walk the map doesn't have, naming those it has.
[[nodiscard]] std::expected<BenchmarkWalk, UsageError> find_benchmark_walk(std::string_view map,
                                                                           std::string_view name);

/// Returns how many frames a benchmark walk draws: one for every 4 ticks it plays, any ticks left
/// over at its end playing without a frame.
[[nodiscard]] std::size_t frame_count(const BenchmarkWalk& walk) noexcept;

/// Where a frame of a benchmark walk is seen from, and what the CPU took to get there.
struct WalkedFrame {
    /// The eye the frame is drawn from.
    Eye eye;

    /// The CPU time the frame's ticks took, with placing its eye.
    std::chrono::nanoseconds simulation{};
};

/// Walks a benchmark walk with the controller, from the map's start position, without drawing it.
///
/// \param map The map the walk is for.
/// \param walk The walk.
/// \return Each frame of the walk, in order.
[[nodiscard]] std::vector<WalkedFrame> walk_benchmark(const wulfhilde::Map& map,
                                                      const BenchmarkWalk& walk);

} // namespace viewer
