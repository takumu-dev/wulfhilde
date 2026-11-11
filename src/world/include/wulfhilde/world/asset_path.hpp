/// \file
/// The syntax of an asset path, which names an asset under its content root.

#pragma once

#include <string_view>

namespace wulfhilde {

/// Tells whether a string is an asset path: `/`-separated segments of lowercase `a`–`z`, `0`–`9`,
/// `_` and `-`, none of them empty.
///
/// An asset path is relative to its content root, and has no extension, since the kind of asset
/// expected where it is written supplies one. It so can't leave its content root, and names the
/// same file on case-sensitive and case-insensitive filesystems alike.
///
/// \param path The string.
/// \return Whether it is an asset path.
[[nodiscard]] bool is_asset_path(std::string_view path) noexcept;

} // namespace wulfhilde
