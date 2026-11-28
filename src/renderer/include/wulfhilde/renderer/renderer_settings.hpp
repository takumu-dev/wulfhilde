/// \file
/// The settings a renderer is created with.

#pragma once

#include <cstddef>
#include <optional>

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

    /// How many times anisotropic filtering samples a texture, from 1 to 16, fixed for the
    /// renderer's life.
    ///
    /// Textures are filtered trilinearly between their mip levels, and anisotropically along the
    /// longer side of a pixel's footprint, so a floor or a wall seen at a grazing angle stays sharp
    /// where trilinear filtering alone would blur it. 1 is trilinear filtering alone. Every device
    /// the renderer comes up on takes 16. Any other value breaks a precondition of the renderer's
    /// creation.
    std::size_t anisotropy = 16;

    /// How fast exposure follows what the camera sees, 0 or more, per second.
    ///
    /// Each frame or image is metered from a histogram of its own light, within the map's exposure
    /// range, and the exposure moves toward what it meters, so a sunlit yard and a dark corridor
    /// both stay readable. A second later, e to the minus this of the gap is left: at 2, about
    /// 14%. Infinity takes each frame's metered exposure at once, and 0 never moves from the
    /// first. The first frame or image after a map is set always takes its own. Any value below 0,
    /// or not a number, breaks a precondition of the renderer's creation.
    float exposure_adaptation_speed = 2.0F;

    /// An exposure to see every frame and image at, in EV100, in place of automatic exposure, or
    /// none to expose automatically.
    ///
    /// The map's exposure range and compensation don't apply to it. A game's tests that measure
    /// light pin it, so that brighter light shows brighter. A value that isn't a finite number
    /// breaks a precondition of the renderer's creation.
    std::optional<float> pinned_exposure{};

    /// Whether the renderer measures frames, timing each of their passes on the GPU with timestamp
    /// queries, as `Renderer::measure_frame` does.
    ///
    /// Not every device has timestamp queries, and the renderer needs none to draw, so only when
    /// this is on is a device without them unfit: creating the renderer then fails, naming them
    /// among what each device lacks.
    bool measures_frames = false;

    /// How deep views through portals nest at most, each within the opening of the one it is seen
    /// from.
    ///
    /// A view starts at a transformed portal, or at an ordinary portal into space that another
    /// sector shares. Ordinary portals within a view nest nothing. Two portals that see each other,
    /// as facing mirrors do, nest views without end, and past this limit the opening that would
    /// start a view is filled flat, with the ambient light of the sector beyond as a grey surface
    /// reflecting 18% of it would give back, so that it keeps its brightness under automatic
    /// exposure. 0 fills every such opening seen from the camera's own view.
    std::size_t nested_view_limit = 4;

    /// How many views a frame draws at most, the camera's own among them, 1 or more.
    ///
    /// Views are taken depth first, each followed by those nested in it, and past this limit the
    /// opening that would start a view is filled flat, as past `nested_view_limit`. Any value below
    /// 1 breaks a precondition of the renderer's creation.
    std::size_t view_limit = 16;
};

} // namespace wulfhilde
