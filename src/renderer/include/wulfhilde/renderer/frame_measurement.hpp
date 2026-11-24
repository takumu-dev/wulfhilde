/// \file
/// What a frame cost to draw, as a renderer that measures its frames measures it.

#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>
#include <vector>

namespace wulfhilde {

/// The GPU time of one of a frame's passes.
struct PassTime {
    /// The pass's name, which lives as long as the program.
    std::string_view name{};

    /// How long the GPU took over the pass, from the end of the pass before it.
    std::chrono::nanoseconds time{};
};

/// What a frame cost to draw, on the CPU and on the GPU, and how much it drew.
struct FrameMeasurement {
    /// The CPU time the renderer took to ready the frame's work, record it and submit it to the
    /// GPU, without waiting for the GPU.
    std::chrono::nanoseconds submission{};

    /// The GPU time the frame's work took, from the start of its first pass to the end of its last.
    std::chrono::nanoseconds gpu{};

    /// The GPU time of each pass, in the order they run, which add up to `gpu`. Every frame of a
    /// renderer has the same passes, in the same order, and a pass with nothing to do takes no
    /// time.
    std::vector<PassTime> passes{};

    /// How many views the frame drew: one for the camera, and one for each view through a portal.
    std::size_t views = 0;

    /// How many shadow maps the frame rendered.
    std::size_t shadow_maps = 0;

    /// How many draws the frame's views recorded for the map's surfaces.
    std::size_t draws = 0;

    /// The device memory the renderer held once the frame was done, in bytes: what it allocated
    /// from the device for its buffers and images.
    std::size_t device_memory = 0;
};

} // namespace wulfhilde
