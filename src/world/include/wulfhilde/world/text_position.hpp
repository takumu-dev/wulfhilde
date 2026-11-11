/// \file
/// Where in a file's text something lies.

#pragma once

#include <cstddef>

namespace wulfhilde {

/// Where in a file's text something lies, as a person counts it, and as compilers and editors
/// write it: a line, and a column within the line.
struct TextPosition {
    /// The line, counting from 1.
    std::size_t line = 0;

    /// The column, counting from 1 in Unicode code points, a tab being one.
    std::size_t column = 0;

    /// Compares two positions exactly.
    constexpr bool operator==(const TextPosition&) const noexcept = default;
};

} // namespace wulfhilde
