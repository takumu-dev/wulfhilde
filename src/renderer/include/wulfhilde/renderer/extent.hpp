/// \file
/// The size of an image in pixels.

#pragma once

#include <cstddef>

namespace wulfhilde {

/// The width and height of an image, in pixels.
struct Extent {
    /// The number of pixels in each row.
    std::size_t width = 0;

    /// The number of rows.
    std::size_t height = 0;

    /// Compares two sizes exactly.
    constexpr bool operator==(const Extent&) const noexcept = default;
};

} // namespace wulfhilde
