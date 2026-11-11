/// \file
/// The assets a map uses, loaded and ready for the renderer.

#pragma once

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/assets/texture_image.hpp>

#include <vector>

namespace wulfhilde {

/// The assets a map uses, loaded and ready for the renderer.
struct MapAssets {
    /// The map's materials, each at the index of its `MaterialId`.
    std::vector<Material> materials{};

    /// The images the materials read: each distinct image once for each way it is read, in the
    /// order the materials first read them.
    std::vector<TextureImage> images{};

    /// Compares two sets of assets exactly.
    bool operator==(const MapAssets&) const = default;
};

} // namespace wulfhilde
