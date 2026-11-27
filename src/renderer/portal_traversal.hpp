// What a camera sees of a map, found on the CPU by walking through the portals from the camera's
// sector: a plain record for each of a frame's views, and one for each sector seen in each view,
// with the window of the screen it is seen through.
#pragma once

#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>

#include <cstddef>
#include <vector>

namespace wulfhilde::detail {

/// A rectangle of the screen, in Vulkan's normalised device coordinates: x from -1 at the image's
/// left edge to 1 at its right, and y from -1 at its top to 1 at its bottom. The whole screen by
/// default.
struct ScreenWindow {
    Vector2 min{.x = -1.0F, .y = -1.0F};
    Vector2 max{.x = 1.0F, .y = 1.0F};

    /// Compares two windows exactly.
    constexpr bool operator==(const ScreenWindow&) const noexcept = default;
};

/// What one camera sees: for now, only the frame's own camera, in its sector.
struct ViewRecord {
    /// From the coordinates of the view's sector into Vulkan's clip space: x to the right, y down,
    /// and depth from 1 at the near plane to 0 infinitely far away.
    Matrix4 view_projection;
};

/// A sector seen in a view, and the part of the screen it may be seen in, through the openings
/// that lead to it.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its sector has none
struct SectorRecord {
    /// The view, by its index among the frame's.
    std::size_t view = 0;

    /// The sector.
    SectorId sector;

    /// The window of the screen the sector may be seen in.
    ScreenWindow window;
};

/// A frame's views, and the sectors seen in each, in order of view and then of sector ID.
struct PortalTraversal {
    std::vector<ViewRecord> views;
    std::vector<SectorRecord> sectors;
};

/// Finds what a camera sees of a map: its one view, and the sectors seen in it.
///
/// The camera's sector is seen across the whole screen. From each sector seen, the walk goes on
/// through each opening of its walls into the sector beyond, which is seen where the opening is
/// seen within its own sector's window, so that windows shrink at each opening, and a sector whose
/// openings all lie outside its window leads nowhere. A sector reached through several openings is
/// seen within the rectangle that bounds their windows. Windows are rounded out to steps of about
/// a pixel, so they hold every pixel they touch. An opening in a plane nearer the eye than the
/// near plane passes its sector's window on whole, since the eye may stand in it.
///
/// \param map The map.
/// \param camera Where the frame is seen from, in a sector of the map.
/// \param extent The frame's size, neither side of it zero.
[[nodiscard]] PortalTraversal traverse_portals(const Map& map, const Camera& camera, Extent extent);

} // namespace wulfhilde::detail
