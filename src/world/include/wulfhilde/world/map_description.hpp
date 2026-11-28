/// \file
/// The plain description of a map, as its file writes it, from which a map is made.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/exposure.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace wulfhilde {

/// What a surface carries: the material that covers it, and how the material lies on it.
///
/// The material's textures repeat at the material's own real-world size, times `scale`. On a wall,
/// u runs along the wall from its start, to the right as the wall is seen from its sector, and v is
/// height, so textures line up across neighbouring walls. On a floor or a ceiling, u and v run
/// along X and Y.
struct SurfaceDescription {
    /// The asset path of the material: `/`-separated segments of lowercase `a`–`z`, `0`–`9`, `_`
    /// and `-`, relative to the content root and without an extension.
    std::string material{};

    /// How far the material is moved along u and v, in metres.
    Vector2 offset{};

    /// What the material's size is multiplied by, along u and v.
    Vector2 scale{.x = 1.0F, .y = 1.0F};

    /// How far the material is turned in the surface's plane, from +u toward +v, about the point
    /// `offset` moves its origin to. `scale` applies along the material's own turned axes.
    Angle rotation{};
};

/// A wall of a loop, which runs from its own start to the start of the next wall in the loop.
struct WallDescription {
    /// The wall's name, unique among the map's walls, by which a portal refers to it, or nothing
    /// for a wall that nothing refers to.
    std::optional<std::string> id{};

    /// Where the wall starts, seen from above, in metres.
    Vector2 start{};

    /// The wall's surface, from the sector's floor to its ceiling. A wall that is a side of a
    /// portal shows it only on its strips, unless they have surfaces of their own.
    SurfaceDescription surface{};

    /// The surface of the wall's upper strip, above its opening where the sector beyond has a lower
    /// ceiling, or nothing to take `surface`. It applies only to a side of a portal.
    std::optional<SurfaceDescription> upper{};

    /// The surface of the wall's lower strip, below its opening where the sector beyond has a
    /// higher floor, or nothing to take `surface`. It applies only to a side of a portal.
    std::optional<SurfaceDescription> lower{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

/// A floor or ceiling that shows an infinitely distant backdrop instead of a material.
///
/// The backdrop is fixed to the sector's coordinates: its image's centre column faces +X, and its
/// top row is straight up.
struct SkyDescription {
    /// The asset path of the backdrop, as in `SurfaceDescription::material`.
    std::string backdrop{};

    /// The luminance of the backdrop's white, in nits, or nothing to take the backdrop's own.
    std::optional<float> nits{};
};

/// A sector's floor or ceiling: the height it lies at, and what covers it.
///
/// Its surface's fields sit beside its height, as the file writes them, and a sky takes the place
/// of all of them.
struct FloorOrCeilingDescription {
    /// The height it lies at, in metres.
    float height = 0.0F;

    /// The asset path of the material that covers it, as in `SurfaceDescription::material`.
    std::string material{};

    /// How far the material is moved along X and Y, in metres.
    Vector2 offset{};

    /// What the material's size is multiplied by, along X and Y.
    Vector2 scale{.x = 1.0F, .y = 1.0F};

    /// How far the material is turned in the surface's plane, from +X toward +Y, about the point
    /// `offset` moves its origin to. `scale` applies along the material's own turned axes.
    Angle rotation{};

    /// The sky it shows instead of a material, or nothing for a surface. With a sky, the fields of
    /// the material don't apply.
    std::optional<SkyDescription> sky{};
};

/// A sector: a region bounded by an outer loop of walls and any inner loops around holes in it,
/// with a floor below and a ceiling above.
struct SectorDescription {
    /// The sector's name, unique in the map, by which the start position refers to it.
    std::string id{};

    /// The sector's floor.
    FloorOrCeilingDescription floor{};

    /// The sector's ceiling.
    FloorOrCeilingDescription ceiling{};

    /// The sector's ambient light, or nothing to take the map's.
    std::optional<AmbientLight> ambient{};

    /// The sector's loops, each a list of walls in order: first its outer loop, which runs
    /// anticlockwise seen from above, then any inner loops, each around a hole in the sector and
    /// running clockwise, so that every wall has the sector to its left. No loop crosses or touches
    /// itself or another, each inner loop lies inside the outer loop, and none lies inside another.
    /// Any loop may be concave. An inner loop of solid walls is a pillar, and one whose walls are
    /// sides of portals leads into the sector inside it.
    std::vector<std::vector<WallDescription>> loops{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

/// Which kind of surface a side of a portal is.
enum class PortalSideKind : std::uint8_t {
    wall,    ///< A wall, named by its id.
    floor,   ///< A sector's floor, named by the sector's id.
    ceiling, ///< A sector's ceiling, named by the sector's id.
};

/// One side of a portal: a wall, or a sector's floor or ceiling.
///
/// The file writes it as an object of one member, named after its kind, whose value is the id,
/// such as `{ "wall": "hall-door" }`.
struct PortalSideDescription {
    /// Which kind of surface the side is.
    PortalSideKind kind = PortalSideKind::wall;

    /// The id of the wall, or of the sector whose floor or ceiling the side is.
    std::string id{};
};

/// A portal, which links one side to the other, so that each side's sector is seen and reached
/// through it from the other's.
///
/// Walls link only to walls. An ordinary wall portal links two walls of different sectors that
/// run between the same corners in opposite directions, and a wall is a side of one portal at
/// most. This version of the engine makes wall portals only.
struct PortalDescription {
    /// One side of the portal.
    PortalSideDescription a{};

    /// The other side of the portal.
    PortalSideDescription b{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

/// Where and facing which way the player is placed when the map loads.
///
/// The facing is turned from +X by `yaw`, then `pitch`, as everywhere in the engine.
struct StartPositionDescription {
    /// The id of the sector the player starts in.
    std::string sector{};

    /// Where the player's feet are, in that sector's coordinates, in metres.
    Vector3 point{};

    /// How far the facing turns left from +X, anticlockwise seen from above.
    Angle yaw{};

    /// How far the facing tilts up from the horizontal.
    Angle pitch{};
};

/// A map as its file writes it, in plain values, from which `Map::create` makes a map.
///
/// It mirrors the file, so the path through it to each value is that value's JSON pointer:
/// `sectors[0].loops[0][2].start.y` is `/sectors/0/loops/0/2/start/1`. A game builds one in code,
/// for its tests and tools, and the engine's loader decodes a file into one. Units are metres, in
/// right-handed coordinates with Z up.
struct MapDescription {
    /// The ambient light of every sector that doesn't set its own, or nothing for black.
    std::optional<AmbientLight> ambient{};

    /// The exposures the map is seen at.
    Exposure exposure{};

    /// The map's sectors.
    std::vector<SectorDescription> sectors{};

    /// The map's portals, which link sectors.
    std::vector<PortalDescription> portals{};

    /// Where the player starts.
    StartPositionDescription start{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
