/// \file
/// A typed ID, by which modules refer to the objects of a map.

#pragma once

#include <compare>
#include <cstdint>

namespace wulfhilde {

/// The ID of an object of one kind, such as a sector or a wall: its index in the tables of the map
/// that issued it, in the order the map's file lists them.
///
/// The tag makes each kind of ID a type of its own, so a sector's ID can't be passed where a
/// wall's is wanted. "No object" is an empty `std::optional`, never a value set aside.
///
/// \tparam Tag A type that names the kind of object, such as `struct SectorTag`, which needs no
///             definition.
template <typename Tag> class Id {
  public:
    /// Makes the ID of the object at an index.
    ///
    /// \param index The object's index in the tables of the map that issues the ID.
    constexpr explicit Id(std::uint32_t index) noexcept : index_(index) {}

    /// Returns the object's index in the tables of the map that issued the ID.
    [[nodiscard]] constexpr std::uint32_t index() const noexcept { return index_; }

    /// Compares and orders two IDs of the same kind by their indices.
    constexpr auto operator<=>(const Id&) const noexcept = default;

  private:
    std::uint32_t index_;
};

} // namespace wulfhilde
