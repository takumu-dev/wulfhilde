/// \file
/// The settings a renderer is created with.

#pragma once

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
};

} // namespace wulfhilde
