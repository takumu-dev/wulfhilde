/// \file
/// The checked narrowing of one integer type to another.

#pragma once

#include <wulfhilde/core/assertion.hpp>

#include <concepts>
#include <utility>

namespace wulfhilde {

/// Converts an integer to another integer type, which must hold its value exactly.
///
/// Indices and counts are `std::size_t`, and this is how they reach the narrower types that IDs
/// and outside interfaces take. A value the type can't hold breaks the precondition, which is
/// checked in every build. `bool` and the character types aren't integers here.
///
/// \tparam To The integer type to convert to.
/// \tparam From The integer type to convert from.
/// \param value The integer to convert, which `To` must be able to hold.
/// \return The same value as a `To`.
template <std::integral To, std::integral From> [[nodiscard]] constexpr To narrow(From value) {
    WULFHILDE_ASSERT(std::in_range<To>(value), "the value fits the type it is narrowed to");
    return static_cast<To>(value);
}

} // namespace wulfhilde
