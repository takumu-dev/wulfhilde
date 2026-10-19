/// \file
/// A content root: the directory that holds the assets maps use.

#pragma once

#include <wulfhilde/assets/asset_error.hpp>

#include <expected>
#include <filesystem>
#include <utility>

namespace wulfhilde {

/// A directory that holds the assets maps use, the maps included, against which every asset path is
/// resolved.
///
/// It needs no layout inside it. An asset path names a file under it without an extension, which
/// the kind of asset expected where the path is written supplies: `.map.json` for a map. Symbolic
/// links are followed, inside the directory and to it, since a content root is a developer's own
/// content rather than a boundary to keep anything within.
class ContentRoot {
  public:
    /// Opens a directory as a content root.
    ///
    /// \param directory The directory, absolute or relative to the working directory.
    /// \return The content root, or why the directory can't be one.
    [[nodiscard]] static std::expected<ContentRoot, AssetError>
    open(const std::filesystem::path& directory);

    /// Returns the directory, as an absolute path, so that a change of the working directory
    /// leaves it where it was.
    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

  private:
    /// Makes a content root of a directory that `open` found.
    ///
    /// \param directory The directory, as an absolute path.
    explicit ContentRoot(std::filesystem::path directory) : directory_(std::move(directory)) {}

    /// The directory, as an absolute path.
    std::filesystem::path directory_;
};

} // namespace wulfhilde
