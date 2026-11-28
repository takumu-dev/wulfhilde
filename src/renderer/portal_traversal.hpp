// What a camera sees of a map, found on the CPU by walking through the portals from the camera's
// sector: a plain record for each of a frame's views, and one for each sector seen in each view,
// with the window of the screen it is seen through.
#pragma once

#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/plane.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace wulfhilde::detail {

/// How deep views nest within the main view at most: past it, an opening that would start a view
/// leads nowhere.
inline constexpr std::size_t nested_view_limit = 4;

/// How many views a frame has at most, the main view among them: past it, an opening that would
/// start a view leads nowhere.
inline constexpr std::size_t view_limit = 16;

/// A rectangle of the screen, in Vulkan's normalised device coordinates: x from -1 at the image's
/// left edge to 1 at its right, and y from -1 at its top to 1 at its bottom. The whole screen by
/// default.
struct ScreenWindow {
    Vector2 min{.x = -1.0F, .y = -1.0F};
    Vector2 max{.x = 1.0F, .y = 1.0F};

    /// Compares two windows exactly.
    constexpr bool operator==(const ScreenWindow&) const noexcept = default;
};

/// What one camera sees: the main view, from the frame's own camera in its sector, or a view that
/// starts at a portal into space that another sector shares, confined to its opening.
struct ViewRecord {
    /// From the coordinates of the view's sectors into Vulkan's clip space: x to the right, y down,
    /// and depth from 1 at the near plane to 0 infinitely far away.
    Matrix4 view_projection;

    /// Where the eye is, in the coordinates of the view's sectors, which skies are seen from.
    Vector3 eye;

    /// The view it is seen from, by its index among the frame's, which comes before it, or nothing
    /// for the main view.
    std::optional<std::size_t> parent{};

    /// How many views it is nested in, the main view among them: 0 for the main view.
    std::size_t depth = 0;

    /// The plane of the portal it starts at, facing beyond it, where its surfaces are drawn, so
    /// that none between the eye and the portal are: nothing for the main view, which draws all.
    std::optional<Plane> clip{};

    /// The corners of the opening it is seen through, in order round it, in the coordinates of the
    /// view it is seen from: none for the main view.
    std::array<Vector3, 4> opening{};

    /// Whether the opening's plane lies so near the eye that the near plane would take away some
    /// of what the screen shows of it, as when the eye stands in it.
    bool opening_at_eye = false;
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

/// A frame's views, each after the view it is seen from and the views nested in that view's views
/// before it, and the sectors seen in each, in order of view and then of sector ID.
struct PortalTraversal {
    std::vector<ViewRecord> views;
    std::vector<SectorRecord> sectors;
};

/// Finds what a camera sees of a map: its views, and the sectors seen in each.
///
/// The camera's sector is seen across the whole screen, in the main view. From each sector seen,
/// the walk goes on through each opening of its walls that faces the eye into the sector beyond,
/// which is seen where the opening is seen within its own sector's window, so that windows shrink
/// at each opening, and a sector whose openings all lie outside its window leads nowhere. A sector
/// reached through several openings is seen within the rectangle that bounds their windows.
/// Windows are rounded out to steps of about a pixel, so they hold every pixel they touch. An
/// opening in a plane nearer the eye than the near plane passes its sector's window on whole,
/// since the eye may stand in it.
///
/// A sector beyond an opening that shares space with any other sector can't be told apart from it
/// by depth, so the walk doesn't take it into the view. It starts a view of its own there, nested
/// in the view the opening is seen in, within the opening's window, which never leads back through
/// that opening. Each such opening starts one view, after the walk through the view it is seen in
/// is done, until views nest as deep as the limit or the frame has as many as it may.
///
/// \param map The map.
/// \param camera Where the frame is seen from, in a sector of the map.
/// \param extent The frame's size, neither side of it zero.
[[nodiscard]] PortalTraversal traverse_portals(const Map& map, const Camera& camera, Extent extent);

} // namespace wulfhilde::detail
