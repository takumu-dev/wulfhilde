// What a renderer draws with on its device, and how it draws a map: its pipelines, the geometry of
// the map that is set, the targets of the size last drawn, and the exposure it has adapted to.
#pragma once

#include "bindless_descriptors.hpp"
#include "device.hpp"
#include "owned_handle.hpp"
#include "presentation.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace wulfhilde::detail {

/// Draws a map on one device: each sector's surfaces into a multisampled scene in linear light,
/// textured and filtered anisotropically, then metering, which moves the exposure toward what a
/// histogram of the scene's light meters, then the AgX pass, which resolves the scene's samples
/// into an sRGB image, or into a window's image to show. An image is drawn as a frame is.
class Drawing {
    struct Pipelines;
    struct Metering;
    struct Geometry;
    struct Targets;

    // Lets `create` alone call the public constructor that std::make_unique needs.
    struct Key {
        explicit Key() = default;
    };

  public:
    /// Makes every pipeline for a sample count that the device takes for its scene targets, and
    /// for the format of a window's images if there is a window, the sampler that reads every
    /// texture, with the settings' anisotropic filtering, what metering keeps between frames, to
    /// expose as the settings say, and the timestamps that measure a frame if the settings ask.
    [[nodiscard]] static std::expected<std::unique_ptr<Drawing>, DeviceError>
    create(Device& device, VkSampleCountFlagBits samples, const RendererSettings& settings,
           std::optional<WindowFormat> window_format = std::nullopt);

    /// Takes the pipelines, the sampler, what metering keeps and the timestamps that `create`
    /// made, with the sampler's place in the bindless set.
    Drawing(Key /*key*/, VkSampleCountFlagBits samples, std::unique_ptr<Pipelines> pipelines,
            SamplerHandle texture_sampler, DescriptorHandle sampler_descriptor,
            std::unique_ptr<Metering> metering, QueryPoolHandle timestamps) noexcept;

    Drawing(const Drawing&) = delete;
    Drawing(Drawing&&) = delete;
    Drawing& operator=(const Drawing&) = delete;
    Drawing& operator=(Drawing&&) = delete;
    ~Drawing();

    /// Triangulates a map and uploads its geometry, its shading and its textures with their mip
    /// chains to the device this was made on, in place of any set before, and meters within its
    /// exposure range from the next frame on, taking that frame's exposure at once.
    [[nodiscard]] std::expected<void, DeviceError> set_map(Device& device, const Map& map,
                                                           const MapAssets& assets);

    /// Draws the map that is set as a camera sees it, on the device this was made on, and reads
    /// the image back.
    [[nodiscard]] std::expected<Image, DeviceError>
    render_image(Device& device, const Camera& camera, Extent extent);

    /// Whether this measures frames, which only one made with timestamps does.
    [[nodiscard]] bool measures_frames() const noexcept {
        return timestamps_.get() != VK_NULL_HANDLE;
    }

    /// Draws the map that is set as a camera sees it, on the device this was made on, into the
    /// output image, which is never read back, and measures what that cost once it is done.
    [[nodiscard]] std::expected<FrameMeasurement, DeviceError>
    measure_frame(Device& device, const Camera& camera, Extent extent);

    /// Draws the map that is set as a camera sees it into a window's image, on the device this was
    /// made on, once the image is acquired, leaving it ready to present.
    [[nodiscard]] std::expected<void, DeviceError> draw(Device& device, const Camera& camera,
                                                        const Frame& frame);

  private:
    // Makes the targets anew unless they are of the extent already.
    [[nodiscard]] std::expected<void, DeviceError> fit_targets(Device& device, Extent extent);

    // How far the exposure moves toward what the next frame meters: all the way for the first
    // frame after a map is set, and otherwise as far as the adaptation speed takes it in the time
    // since the last frame, which it notes.
    [[nodiscard]] float next_blend();

    // Records the scene's pass, then metering, unless the exposure is pinned, moving the exposure
    // by a blend toward what it meters, then the AgX pass with a pipeline into an output image's
    // view, which is in the colour attachment layout and stays so. With timestamps, it writes one
    // before the passes and one after each.
    void record_passes(const Device& device, VkCommandBuffer commands, const Camera& camera,
                       float blend, VkPipeline resolve, VkImageView output,
                       VkQueryPool timestamps = VK_NULL_HANDLE) const;

    VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
    std::unique_ptr<Pipelines> pipelines_;
    SamplerHandle texture_sampler_;
    DescriptorHandle sampler_descriptor_;
    std::unique_ptr<Metering> metering_;
    // The timestamps a measured frame writes, or none when this doesn't measure frames.
    QueryPoolHandle timestamps_;
    std::unique_ptr<Geometry> geometry_;
    std::unique_ptr<Targets> targets_;
    // The place of the scene's colour target in the bindless set, kept for the targets of each new
    // size, so that resizing never fills the set.
    std::optional<DescriptorHandle> scene_descriptor_;
    // The places of the textures in the bindless set, kept for the textures of each map set after,
    // so that setting maps never fills the set.
    std::vector<DescriptorHandle> texture_descriptors_;
};

} // namespace wulfhilde::detail
