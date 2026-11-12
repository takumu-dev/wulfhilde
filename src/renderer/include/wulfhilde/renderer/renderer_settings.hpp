/// \file
/// The settings a renderer is created with.

#pragma once

#include <cstddef>

namespace wulfhilde {

/// How a renderer is set up when it is created.
struct RendererSettings {
    /// Whether the renderer runs under Vulkan's validation layers.
    ///
    /// Each message the layers send reaches the log sink, at `warning` for a performance or
    /// best-practice hint and at `error` for misuse of Vulkan, which is a bug in the engine.
    /// Creating a renderer fails when this is on and the layers aren't installed. They slow every
    /// frame, so a game turns this on for development builds and tests only.
    bool validation = false;

    /// How many samples each pixel takes for anti-aliasing, fixed for the renderer's life.
    ///
    /// Every pixel is shaded once, and its samples are tone-mapped one by one before they are
    /// averaged, so edges against bright light stay smooth. 4 is the only count above 1 that Vulkan
    /// guarantees. Any count the device supports for both the renderer's colour target and its
    /// depth and stencil is accepted, and any other fails the renderer's creation.
    std::size_t sample_count = 4;
};

} // namespace wulfhilde
