/// \file
/// The size of a window.

#pragma once

#include <cstddef>

namespace wulfhilde {

/// The width and height of a window's drawable area, in the desktop's units.
///
/// On most displays a unit is a pixel. A display set to scale everything up has more pixels than
/// units, and the renderer draws at its pixels.
struct WindowSize {
    /// The width.
    std::size_t width = 0;

    /// The height.
    std::size_t height = 0;

    /// Compares two sizes exactly.
    constexpr bool operator==(const WindowSize&) const noexcept = default;
};

} // namespace wulfhilde
