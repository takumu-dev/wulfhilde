/// \file
/// A sector of a map, as the map holds it.

#pragma once

#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/surface.hpp>

#include <string>
#include <vector>

namespace wulfhilde {

/// A sector's floor or ceiling, as a map holds it: the height it lies at, and what covers it.
struct FloorOrCeiling {
    /// The height it lies at, in metres.
    float height = 0.0F;

    /// What covers it, with u and v along X and Y.
    Surface surface;
};

/// A sector of a map: a region bounded by a loop of walls, with a floor below and a ceiling above.
/// Its down points toward its floor.
///
/// A map holds it, and every reference in it is to that map's objects.
struct Sector {
    /// The sector's name, unique in its map, as the map's description gives it.
    std::string id{};

    /// The sector's floor.
    FloorOrCeiling floor;

    /// The sector's ceiling.
    FloorOrCeiling ceiling;

    /// The sector's loops, each the IDs of its walls in order. A sector has one, its outer loop,
    /// which runs anticlockwise seen from above.
    std::vector<std::vector<WallId>> loops{};

    /// The sector's ambient light: its own, or else its map's, or else black.
    AmbientLight ambient{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
