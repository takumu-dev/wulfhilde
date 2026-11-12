// What a renderer draws with on its device, and how it draws a map: its pipelines, the geometry of
// the map that is set, and the targets of the size last drawn.
#pragma once

#include "bindless_descriptors.hpp"
#include "device.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/world/map.hpp>

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

namespace wulfhilde::detail {

/// Draws a map on one device: each sector's surfaces into a multisampled scene in linear light,
/// then the AgX pass, which resolves the scene's samples into an sRGB image.
class Drawing {
    struct Pipelines;
    struct Geometry;
    struct Targets;

    // Lets `create` alone call the public constructor that std::make_unique needs.
    struct Key {
        explicit Key() = default;
    };

  public:
    /// Makes every pipeline for a sample count that the device takes for its scene targets.
    [[nodiscard]] static std::expected<std::unique_ptr<Drawing>, DeviceError>
    create(Device& device, VkSampleCountFlagBits samples);

    /// Takes the pipelines that `create` made.
    Drawing(Key /*key*/, VkSampleCountFlagBits samples,
            std::unique_ptr<Pipelines> pipelines) noexcept;

    Drawing(const Drawing&) = delete;
    Drawing(Drawing&&) = delete;
    Drawing& operator=(const Drawing&) = delete;
    Drawing& operator=(Drawing&&) = delete;
    ~Drawing();

    /// Triangulates a map and uploads its geometry and shading to the device this was made on, in
    /// place of any set before.
    [[nodiscard]] std::expected<void, DeviceError> set_map(const Device& device, const Map& map,
                                                           const MapAssets& assets);

    /// Draws the map that is set as a camera sees it, on the device this was made on, and reads
    /// the image back.
    [[nodiscard]] std::expected<Image, DeviceError>
    render_image(Device& device, const Camera& camera, Extent extent);

  private:
    VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    std::unique_ptr<Pipelines> pipelines_;
    std::unique_ptr<Geometry> geometry_;
    std::unique_ptr<Targets> targets_;
    // The place of the scene's colour target in the bindless set, kept for the targets of each new
    // size, so that resizing never fills the set.
    std::optional<DescriptorHandle> scene_descriptor_;
};

} // namespace wulfhilde::detail
