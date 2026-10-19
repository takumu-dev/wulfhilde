/// \file
/// The plain description of a map, as its file writes it, from which a map is made.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>

#include <optional>
#include <string>
#include <vector>

namespace wulfhilde {

/// What a surface carries: the material that covers it, and how the material lies on it.
///
/// The material's textures repeat at the material's own real-world size, times `scale`. On a wall,
/// u runs along the wall from its start and v is height, so textures line up across neighbouring
/// walls. On a floor or a ceiling, u and v run along X and Y.
struct SurfaceDescription {
    /// The asset path of the material: `/`-separated segments of lowercase `a`–`z`, `0`–`9`, `_`
    /// and `-`, relative to the content root and without an extension.
    std::string material{};

    /// How far the material is moved along u and v, in metres.
    Vector2 offset{};

    /// What the material's size is multiplied by, along u and v.
    Vector2 scale{.x = 1.0F, .y = 1.0F};

    /// How far the material is turned in the surface's plane.
    Angle rotation{};
};

/// A wall of a loop, which runs from its own start to the start of the next wall in the loop.
struct WallDescription {
    /// Where the wall starts, seen from above, in metres.
    Vector2 start{};

    /// The wall's surface, from the sector's floor to its ceiling.
    SurfaceDescription surface{};
};

/// A sector's floor or ceiling: the height it lies at, and what covers it.
///
/// Its surface's fields sit beside its height, as the file writes them.
struct FloorOrCeilingDescription {
    /// The height it lies at, in metres.
    float height = 0.0F;

    /// The asset path of the material that covers it, as in `SurfaceDescription::material`.
    std::string material{};

    /// How far the material is moved along X and Y, in metres.
    Vector2 offset{};

    /// What the material's size is multiplied by, along X and Y.
    Vector2 scale{.x = 1.0F, .y = 1.0F};

    /// How far the material is turned in the surface's plane.
    Angle rotation{};
};

/// A sector: a region bounded by a loop of walls, with a floor below and a ceiling above.
struct SectorDescription {
    /// The sector's name, unique in the map, by which the start position refers to it.
    std::string id{};

    /// The sector's floor.
    FloorOrCeilingDescription floor{};

    /// The sector's ceiling.
    FloorOrCeilingDescription ceiling{};

    /// The sector's ambient light, or nothing to take the map's.
    std::optional<AmbientLight> ambient{};

    /// The sector's loops, each a list of walls in order. A sector has one, its outer loop, which
    /// runs anticlockwise seen from above and doesn't cross itself. It may be concave.
    std::vector<std::vector<WallDescription>> loops{};

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

    /// The map's sectors.
    std::vector<SectorDescription> sectors{};

    /// Where the player starts.
    StartPositionDescription start{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
