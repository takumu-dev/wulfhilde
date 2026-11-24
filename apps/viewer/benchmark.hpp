#pragma once

#include "benchmark_record.hpp"
#include "benchmark_walks.hpp"
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/renderer.hpp>

#include <expected>
#include <span>
#include <vector>

namespace viewer {

/// The size a benchmark draws its frames at: that of the performance targets, 1920x1080.
inline constexpr wulfhilde::Extent benchmark_extent{.width = 1920, .height = 1080};

/// Draws and measures each frame of a benchmark walk.
///
/// \param renderer A renderer made to measure frames, with the walk's map set.
/// \param frames The frames of the walk, as `walk_benchmark` walked them.
/// \param extent The size to draw each frame at.
/// \return What each frame cost, in order, or what failed on the device.
[[nodiscard]] std::expected<std::vector<BenchmarkFrame>, wulfhilde::DeviceError>
play_benchmark(wulfhilde::Renderer& renderer, std::span<const WalkedFrame> frames,
               wulfhilde::Extent extent = benchmark_extent);

/// Names the operating system the viewer runs on, with its version, such as
/// `Linux 6.12.4 x86_64` or `Windows 10.0.26100`.
[[nodiscard]] std::string operating_system();

} // namespace viewer
