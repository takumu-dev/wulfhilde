/// \file
/// A map: a complete world of sectors, immutable and valid.

#pragma once

#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_load_error.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/start_position.hpp>
#include <wulfhilde/world/trace.hpp>
#include <wulfhilde/world/wall.hpp>

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde {

/// A complete world of sectors, made from a description that passed every check.
///
/// A map that exists is valid, so nothing that reads it handles a half-checked one. It never
/// changes, and it refers to its own objects by typed IDs, which index its tables in the order its
/// description lists the objects. It opens no files: the assets module reads a map's file into a
/// description and makes the map from that.
class Map {
  public:
    /// Makes a map from its description, checking every rule a map must keep.
    ///
    /// \param description The map's description.
    /// \return The map, or every problem found in the description.
    [[nodiscard]] static std::expected<Map, MapLoadError> create(MapDescription description);

    /// Returns the map's sectors, each at the index of its ID.
    [[nodiscard]] std::span<const Sector> sectors() const noexcept { return sectors_; }

    /// Returns one of the map's sectors.
    ///
    /// \param sector The sector's ID, which this map issued.
    /// \return The sector.
    [[nodiscard]] const Sector& sector(SectorId sector) const;

    /// Returns one of the map's walls.
    ///
    /// \param wall The wall's ID, which this map issued.
    /// \return The wall.
    [[nodiscard]] const Wall& wall(WallId wall) const;

    /// Returns where the player starts.
    [[nodiscard]] const StartPosition& start() const noexcept { return start_; }

    /// Finds a sector by its id, the name the map's description gives it.
    ///
    /// \param id The sector's id.
    /// \return The sector's ID, or nothing if no sector has that id.
    [[nodiscard]] std::optional<SectorId> find_sector(std::string_view id) const noexcept;

    /// Tells whether a location's point lies in its sector: within the sector's walls seen from
    /// above, and from its floor to its ceiling, all of them included.
    ///
    /// \param location The location, whose sector this map issued.
    /// \return Whether the point is in the sector.
    [[nodiscard]] bool contains(const Location& location) const;

    /// Returns the height of a sector's floor below or above a location's point.
    ///
    /// \param location The location, whose sector this map issued. Its point needn't lie in the
    ///                 sector, and its height doesn't matter.
    /// \return The height of the floor at the point's X and Y, in metres.
    [[nodiscard]] float floor_height(const Location& location) const;

    /// Returns the height of a sector's ceiling below or above a location's point.
    ///
    /// \param location The location, whose sector this map issued. Its point needn't lie in the
    ///                 sector, and its height doesn't matter.
    /// \return The height of the ceiling at the point's X and Y, in metres.
    [[nodiscard]] float ceiling_height(const Location& location) const;

    /// Moves a location in a straight line through the map, stopping at the first surface it meets.
    ///
    /// Surfaces are met from the front only: a wall from inside its sector, a floor moving down and
    /// a ceiling moving up. A trace that starts on a surface and moves into it stops at once, one
    /// that ends exactly on a surface has met it, and one through a corner stops at one of the
    /// corner's walls.
    ///
    /// \param from Where the trace starts, whose sector this map issued.
    /// \param motion How far the trace moves and which way, in the coordinates of the start's
    ///               sector, in metres.
    /// \return Where the trace ended, the transform of the portals it crossed, and the surface that
    ///         stopped it.
    [[nodiscard]] Trace trace(const Location& from, const Vector3& motion) const;

    /// Returns the asset path of each material the map's surfaces carry, each at the index of its
    /// ID.
    [[nodiscard]] std::span<const std::string> materials() const noexcept { return materials_; }

    /// Returns game-defined data that the engine keeps and ignores: the JSON text of an object, or
    /// empty for none.
    [[nodiscard]] const std::string& properties() const noexcept { return properties_; }

  private:
    /// Makes a map with no sectors, for `create` to fill.
    ///
    /// \param start Where the player starts.
    explicit Map(const StartPosition& start) noexcept : start_(start) {}

    /// The sectors, by ID.
    std::vector<Sector> sectors_;

    /// Every sector's ID, ordered by the sector's id, for `find_sector`.
    std::vector<SectorId> sectors_by_name_;

    /// The walls, by ID.
    std::vector<Wall> walls_;

    /// The asset path of each material, by ID.
    std::vector<std::string> materials_;

    /// Where the player starts.
    StartPosition start_;

    /// The map's game-defined data.
    std::string properties_;
};

} // namespace wulfhilde
