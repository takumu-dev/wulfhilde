#include "drawing.hpp"

#include "barriers.hpp"
#include "bindless_descriptors.hpp"
#include "device.hpp"
#include "formats.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "pipelines.hpp"
#include "portal_traversal.hpp"
#include "presentation.hpp"
#include "sector_geometry.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/map.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

#include "culling_spirv.h"
#include "metering_spirv.h"
#include "resolve_spirv.h"
#include "sectors_spirv.h"

// The C++ twin of the sector shaders' `MaterialShading`.
struct MaterialShading {
    Color base_color;
    DescriptorHandle color_texture;
    std::uint32_t sky = 0;
};

// The C++ twin of the sector shaders' `SectorShading`.
struct SectorShading {
    Color ambient;
};

// The C++ twin of the sector shaders' push constants.
struct SceneConstants {
    Matrix4 view_projection;
    std::uint64_t vertices = 0;
    std::uint64_t materials = 0;
    std::uint64_t sectors = 0;
    DescriptorHandle texture_sampler;
    Vector3 eye;
    DescriptorHandle backdrop_sampler;
    float unused = 0.0F;
};

// The C++ twin of the resolve shaders' push constants.
struct ResolveConstants {
    DescriptorHandle scene;
    std::uint64_t ev100 = 0;
};

// The C++ twin of the metering shaders' push constants.
struct MeteringConstants {
    DescriptorHandle scene;
    std::uint64_t histogram = 0;
    std::uint64_t ev100 = 0;
    std::array<std::uint32_t, 2> extent{};
    float min_ev100 = 0.0F;
    float max_ev100 = 0.0F;
    float compensation = 0.0F;
    float blend = 0.0F;
};

// The C++ twin of the culling shader's `SurfaceDraw`.
struct SurfaceDraw {
    std::uint32_t first_vertex = 0;
    std::uint32_t vertex_count = 0;
    Vector3 lowest;
    Vector3 highest;
};

// The C++ twin of the culling shader's `SectorInView`.
struct SectorInView {
    Vector2 window_min;
    Vector2 window_max;
    std::uint32_t first_surface = 0;
    std::uint32_t surface_count = 0;
    std::uint32_t first_candidate = 0;
};

// The C++ twin of the culling shader's `ViewCulling`.
struct ViewCulling {
    Matrix4 view_projection;
    std::uint32_t first_sector = 0;
    std::uint32_t sector_count = 0;
    std::uint32_t first_draw = 0;
    std::uint32_t candidate_count = 0;
};

// The C++ twin of the culling shader's `DrawCommand`, which is Vulkan's own.
struct DrawCommand {
    std::uint32_t vertex_count = 0;
    std::uint32_t instance_count = 0;
    std::uint32_t first_vertex = 0;
    std::uint32_t first_instance = 0;
};

static_assert(sizeof(DrawCommand) == sizeof(VkDrawIndirectCommand));

// The C++ twin of the culling shader's push constants.
struct CullingConstants {
    std::uint64_t views = 0;
    std::uint64_t sectors = 0;
    std::uint64_t surfaces = 0;
    std::uint64_t draws = 0;
    std::uint64_t draw_counts = 0;
};

#include "culling_layout.hpp"  // IWYU pragma: keep
#include "metering_layout.hpp" // IWYU pragma: keep
#include "resolve_layout.hpp"  // IWYU pragma: keep
#include "sectors_layout.hpp"  // IWYU pragma: keep

// The bins of the histogram that metering counts pixels into, as the metering shaders have them.
constexpr std::size_t histogram_bins = 256;

// The pixels on each side of the tile that one group of the histogram's shaders counts, as they
// have it.
constexpr std::uint32_t histogram_tile = 16;

constexpr VkBufferUsageFlags shader_buffer_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

// The passes a measured frame times, in the order they run. A timestamp is written before the
// first and after each.
constexpr std::array<std::string_view, 4> timed_passes{"culling", "main", "exposure", "resolve"};

constexpr auto timestamp_count = narrow<std::uint32_t>(timed_passes.size() + 1);

VkExtent2D vulkan_extent(Extent extent) {
    return {
        .width = narrow<std::uint32_t>(extent.width),
        .height = narrow<std::uint32_t>(extent.height),
    };
}

// Makes a buffer for shaders to read and write that holds a number of values of a type, and at
// least one.
template <typename T>
std::expected<Buffer, DeviceError> buffer_of(const Device& device, std::size_t count,
                                             VkBufferUsageFlags usage = shader_buffer_usage) {
    return Buffer::create(device.allocator(), device.get(),
                          std::max<std::size_t>(count, 1) * sizeof(T), usage);
}

template <typename T>
std::expected<Buffer, DeviceError> upload(const Device& device, std::span<const T> values) {
    auto buffer = buffer_of<T>(device, values.size());
    if (buffer) {
        std::memcpy(buffer->memory().data(), values.data(), values.size_bytes());
    }
    return buffer;
}

void set_viewport(VkCommandBuffer commands, VkExtent2D size) {
    const VkViewport viewport{
        .width = static_cast<float>(size.width),
        .height = static_cast<float>(size.height),
        .maxDepth = 1.0F,
    };
    const VkRect2D scissor{.extent = size};
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
}

constexpr BarrierScope color_attachment_write{
    .stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
};

constexpr BarrierScope depth_attachment_access{
    .stages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
};

// The format a texture image is uploaded in, which decodes its texels as its colour space says
// whenever it is sampled.
VkFormat texture_format(ColorSpace color_space) {
    return color_space == ColorSpace::srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
}

// The image that a material without a colour texture is read through, which changes nothing.
const TextureImage& white_image() {
    static const TextureImage white{
        .color_space = ColorSpace::srgb,
        .levels = {{.width = 1, .height = 1, .texels = {255, 255, 255, 255}}},
    };
    return white;
}

// Makes images on the device and copies their mip chains into them, all in one batch of work,
// leaving each ready for shaders to sample.
std::expected<std::vector<GpuImage>, DeviceError>
upload_textures(Device& device, std::span<const TextureImage* const> images) {
    std::size_t bytes = 0;
    for (const auto* image : images) {
        for (const auto& level : image->levels) {
            bytes += level.texels.size();
        }
    }
    auto staging =
        Buffer::create(device.allocator(), device.get(), bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!staging) {
        return std::unexpected(std::move(staging).error());
    }

    std::vector<GpuImage> textures;
    textures.reserve(images.size());
    // Each image's copies, one for each level, from where its texels lie in the staging buffer.
    std::vector<std::vector<VkBufferImageCopy>> copies;
    copies.reserve(images.size());
    std::size_t offset = 0;
    for (const auto* image : images) {
        WULFHILDE_ASSERT(!image->levels.empty(), "a texture image has at least one mip level");
        const auto& full = image->levels.front();
        auto texture =
            GpuImage::create(device.allocator(), device.get(), texture_format(image->color_space),
                             {.width = full.width, .height = full.height},
                             flags_of(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                             VK_SAMPLE_COUNT_1_BIT, narrow<std::uint32_t>(image->levels.size()));
        if (!texture) {
            return std::unexpected(std::move(texture).error());
        }
        textures.push_back(std::move(*texture));

        auto& regions = copies.emplace_back();
        for (std::uint32_t mip = 0; const auto& level : image->levels) {
            std::memcpy(staging->memory().subspan(offset).data(), level.texels.data(),
                        level.texels.size());
            regions.push_back({
                .bufferOffset = offset,
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = mip,
                        .layerCount = 1,
                    },
                .imageExtent =
                    {
                        .width = narrow<std::uint32_t>(level.width),
                        .height = narrow<std::uint32_t>(level.height),
                        .depth = 1,
                    },
            });
            offset += level.texels.size();
            ++mip;
        }
    }

    constexpr BarrierScope copy_write{
        .stages = VK_PIPELINE_STAGE_2_COPY_BIT,
        .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    };
    const auto executed = device.execute([&](VkCommandBuffer commands) {
        for (const auto& [texture, regions] : std::views::zip(textures, copies)) {
            record_image_barrier(commands, texture.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, no_earlier_work, copy_write);
            vkCmdCopyBufferToImage(commands, staging->get(), texture.get(),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   narrow<std::uint32_t>(regions.size()), regions.data());
            record_image_barrier(commands, texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, copy_write,
                                 {
                                     .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                     .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                 });
        }
    });
    if (!executed) {
        return std::unexpected(executed.error());
    }
    return textures;
}

// Makes a sampler that reads textures, filtered trilinearly between mip levels, and anisotropically
// with up to the given number of samples, where more than one. Textures repeat along u, and along v
// as the address mode says.
std::expected<SamplerHandle, DeviceError>
create_texture_sampler(const Device& device, std::size_t anisotropy,
                       VkSamplerAddressMode address_v = VK_SAMPLER_ADDRESS_MODE_REPEAT) {
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = address_v,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = anisotropy > 1 ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = static_cast<float>(anisotropy),
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateSampler(device.get(), &info, nullptr, &sampler),
                                   "creating a sampler of textures")) {
        return std::unexpected(*error);
    }
    return SamplerHandle(device.get(), sampler);
}

// Makes the timestamps that a measured frame writes.
std::expected<QueryPoolHandle, DeviceError> create_timestamps(const Device& device) {
    const VkQueryPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = timestamp_count,
    };
    VkQueryPool pool = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateQueryPool(device.get(), &info, nullptr, &pool),
                                   "creating the timestamps that measure a frame")) {
        return std::unexpected(*error);
    }
    return QueryPoolHandle(device.get(), pool);
}

// Writes a timestamp once every command recorded before it is done.
void record_timestamp(VkCommandBuffer commands, VkQueryPool timestamps, std::uint32_t index) {
    if (timestamps != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timestamps, index);
    }
}

// Writes what culling reads for a frame's views and the sectors each sees, given each sector's
// surfaces, and returns each view's culling, with the room its draws take, one after another.
std::vector<ViewCulling> write_culling(const PortalTraversal& traversal,
                                       std::span<const Range> sector_surfaces,
                                       const Buffer& view_buffer, const Buffer& sector_buffer) {
    std::vector<ViewCulling> views;
    views.reserve(traversal.views.size());
    for (const auto& view : traversal.views) {
        views.push_back({.view_projection = view.view_projection});
    }
    // The records come view by view, so each view's sectors, and its draws after them, follow the
    // view before's.
    std::vector<SectorInView> sectors;
    sectors.reserve(traversal.sectors.size());
    std::uint32_t draws = 0;
    for (const auto& record : traversal.sectors) {
        auto& view = views.at(record.view);
        if (view.sector_count == 0) {
            view.first_sector = narrow<std::uint32_t>(sectors.size());
            view.first_draw = draws;
        }
        const auto surfaces = sector_surfaces.subspan(record.sector.index(), 1).front();
        sectors.push_back({
            .window_min = record.window.min,
            .window_max = record.window.max,
            .first_surface = narrow<std::uint32_t>(surfaces.first),
            .surface_count = narrow<std::uint32_t>(surfaces.count),
            .first_candidate = view.candidate_count,
        });
        ++view.sector_count;
        view.candidate_count += narrow<std::uint32_t>(surfaces.count);
        draws += narrow<std::uint32_t>(surfaces.count);
    }
    WULFHILDE_DEBUG_ASSERT(std::span(views).size_bytes() <= view_buffer.memory().size() &&
                               std::span(sectors).size_bytes() <= sector_buffer.memory().size(),
                           "the buffers hold the frame's views and sectors");
    std::memcpy(view_buffer.memory().data(), views.data(), std::span(views).size_bytes());
    std::memcpy(sector_buffer.memory().data(), sectors.data(), std::span(sectors).size_bytes());
    return views;
}

} // namespace

struct Drawing::Pipelines {
    // Culling each view's surfaces into its draws.
    PipelineHandle cull;
    PipelineHandle scene;
    // Metering: counting the scene's pixels into the histogram, then the exposure it meters.
    PipelineHandle gather;
    PipelineHandle meter;
    PipelineHandle resolve;
    // The AgX pass into a window's images, or none without a window.
    PipelineHandle present;
};

// What metering keeps between frames: the histogram, which it leaves empty, and the exposure in
// EV100 that each frame is seen at, which it moves, or which stays where it is pinned.
struct Drawing::Metering {
    Buffer histogram;
    Buffer ev100;
    float adaptation_speed = 0.0F;
    bool pinned = false;
    // The exposures of the map that is set.
    Exposure exposure;
    // When the last frame was metered, or none before the first frame after a map is set.
    std::optional<std::chrono::steady_clock::time_point> last_frame;

    // Makes the histogram, empty, and the exposure, where it is pinned if it is.
    static std::expected<std::unique_ptr<Metering>, DeviceError>
    create(const Device& device, const RendererSettings& settings) {
        auto histogram =
            Buffer::create(device.allocator(), device.get(), histogram_bins * sizeof(std::uint32_t),
                           shader_buffer_usage);
        if (!histogram) {
            return std::unexpected(std::move(histogram).error());
        }
        auto ev100 =
            Buffer::create(device.allocator(), device.get(), sizeof(float), shader_buffer_usage);
        if (!ev100) {
            return std::unexpected(std::move(ev100).error());
        }
        std::ranges::fill(histogram->memory(), std::byte{0});
        const float pinned_ev100 = settings.pinned_exposure.value_or(0.0F);
        std::memcpy(ev100->memory().data(), &pinned_ev100, sizeof(pinned_ev100));
        return std::make_unique<Metering>(std::move(*histogram), std::move(*ev100),
                                          settings.exposure_adaptation_speed,
                                          settings.pinned_exposure.has_value());
    }
};

struct Drawing::Geometry {
    // The map, which the renderer's caller keeps alive while it is set.
    const Map* map = nullptr;
    Buffer vertices;
    Buffer materials;
    Buffer sectors;
    // Each surface's vertices and the box that holds them, and each sector's surfaces among them.
    Buffer surfaces;
    std::vector<Range> sector_surfaces;
    // What culling reads and writes for a frame: its views and the sectors each sees, which the CPU
    // writes, then a draw for each surface in sight, and how many draws each view makes. Each holds
    // as many as a frame of one view can need.
    Buffer views;
    Buffer sectors_in_view;
    Buffer draws;
    Buffer draw_counts;
    // The map's images, in the order of its assets, then the white one that stands in for the
    // colour texture of a material without one.
    std::vector<GpuImage> textures;
};

// The targets of one size: the scene's colour and its depth and stencil, multisampled, and the
// image the tone-mapping pass writes, with a buffer to read it back through.
struct Drawing::Targets {
    Extent extent;
    GpuImage scene;
    GpuImage depth;
    GpuImage output;
    Buffer readback;
    DescriptorHandle scene_descriptor;

    // Makes the targets of a size, and puts the scene's colour in the bindless set, in the place
    // it had before if it had one.
    static std::expected<std::unique_ptr<Targets>, DeviceError>
    create(Device& device, Extent extent, VkSampleCountFlagBits samples,
           std::optional<DescriptorHandle>& scene_descriptor) {
        auto scene = GpuImage::create(
            device.allocator(), device.get(), scene_color_format, extent,
            flags_of(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_SAMPLED_BIT), samples);
        if (!scene) {
            return std::unexpected(std::move(scene).error());
        }
        auto depth = GpuImage::create(device.allocator(), device.get(), scene_depth_format, extent,
                                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, samples);
        if (!depth) {
            return std::unexpected(std::move(depth).error());
        }
        auto output = GpuImage::create(
            device.allocator(), device.get(), output_format, extent,
            flags_of(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        if (!output) {
            return std::unexpected(std::move(output).error());
        }
        auto readback = Buffer::create(device.allocator(), device.get(),
                                       extent.width * extent.height * Image::bytes_per_pixel,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (!readback) {
            return std::unexpected(std::move(readback).error());
        }
        DescriptorHandle descriptor{};
        if (scene_descriptor) {
            descriptor = *scene_descriptor;
            device.descriptors().replace_sampled_image(descriptor, scene->view());
        } else {
            descriptor = device.descriptors().add_sampled_image(scene->view());
            scene_descriptor = descriptor;
        }
        return std::make_unique<Targets>(extent, std::move(*scene), std::move(*depth),
                                         std::move(*output), std::move(*readback), descriptor);
    }
};

namespace {

// Makes the AgX pass for a sample count, writing an 8-bit format that encodes sRGB as it stores
// it, or one into which the pass encodes it.
std::expected<PipelineHandle, DeviceError>
create_resolve_pipeline(const Device& device, VkSampleCountFlagBits samples, WindowFormat format) {
    // A scene of one sample a pixel isn't a multisampled image, so it is read another way.
    const std::array<std::uint32_t, 2> constants{
        static_cast<std::uint32_t>(samples),
        format.encodes_srgb ? VK_TRUE : VK_FALSE,
    };
    return create_graphics_pipeline(
        device, {
                    .spirv = resolve_spirv,
                    .vertex_entry_point = "cover",
                    .fragment_entry_point =
                        samples == VK_SAMPLE_COUNT_1_BIT ? "resolve_sample" : "resolve_samples",
                    .specialization_constants = constants,
                    .color_format = format.format,
                });
}

} // namespace

std::expected<std::unique_ptr<Drawing>, DeviceError>
Drawing::create(Device& device, VkSampleCountFlagBits samples, const RendererSettings& settings,
                std::optional<WindowFormat> window_format) {
    auto scene = create_graphics_pipeline(device, {
                                                      .spirv = sectors_spirv,
                                                      .vertex_entry_point = "place",
                                                      .fragment_entry_point = "shade",
                                                      .color_format = scene_color_format,
                                                      .depth_stencil_format = scene_depth_format,
                                                      .samples = samples,
                                                      .cull_mode = VK_CULL_MODE_BACK_BIT,
                                                  });
    if (!scene) {
        return std::unexpected(std::move(scene).error());
    }
    auto cull = create_compute_pipeline(device, culling_spirv, "cull");
    if (!cull) {
        return std::unexpected(std::move(cull).error());
    }
    // A scene of one sample a pixel isn't a multisampled image, so it is read another way.
    auto gather = create_compute_pipeline(device, metering_spirv,
                                          samples == VK_SAMPLE_COUNT_1_BIT ? "gather_sample"
                                                                           : "gather_samples");
    if (!gather) {
        return std::unexpected(std::move(gather).error());
    }
    auto meter = create_compute_pipeline(device, metering_spirv, "meter_exposure");
    if (!meter) {
        return std::unexpected(std::move(meter).error());
    }
    auto resolve = create_resolve_pipeline(device, samples, {.format = output_format});
    if (!resolve) {
        return std::unexpected(std::move(resolve).error());
    }
    PipelineHandle present;
    if (window_format) {
        auto made = create_resolve_pipeline(device, samples, *window_format);
        if (!made) {
            return std::unexpected(std::move(made).error());
        }
        present = std::move(*made);
    }
    auto sampler = create_texture_sampler(device, settings.anisotropy);
    if (!sampler) {
        return std::unexpected(std::move(sampler).error());
    }
    // A backdrop's image goes once around, but only from straight up to straight down.
    auto backdrop_sampler =
        create_texture_sampler(device, settings.anisotropy, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    if (!backdrop_sampler) {
        return std::unexpected(std::move(backdrop_sampler).error());
    }
    auto metering = Metering::create(device, settings);
    if (!metering) {
        return std::unexpected(std::move(metering).error());
    }
    QueryPoolHandle timestamps;
    if (settings.measures_frames) {
        auto made = create_timestamps(device);
        if (!made) {
            return std::unexpected(std::move(made).error());
        }
        timestamps = std::move(*made);
    }
    const auto sampler_descriptor = device.descriptors().add_sampler(sampler->get());
    const auto backdrop_sampler_descriptor =
        device.descriptors().add_sampler(backdrop_sampler->get());
    return std::make_unique<Drawing>(
        Key{}, samples,
        std::make_unique<Pipelines>(std::move(*cull), std::move(*scene), std::move(*gather),
                                    std::move(*meter), std::move(*resolve), std::move(present)),
        std::move(*sampler), sampler_descriptor, std::move(*backdrop_sampler),
        backdrop_sampler_descriptor, std::move(*metering), std::move(timestamps));
}

Drawing::Drawing(Key /*key*/, VkSampleCountFlagBits samples, std::unique_ptr<Pipelines> pipelines,
                 SamplerHandle texture_sampler, DescriptorHandle sampler_descriptor,
                 SamplerHandle backdrop_sampler, DescriptorHandle backdrop_sampler_descriptor,
                 std::unique_ptr<Metering> metering, QueryPoolHandle timestamps) noexcept
    : samples_(samples), pipelines_(std::move(pipelines)),
      texture_sampler_(std::move(texture_sampler)), sampler_descriptor_(sampler_descriptor),
      backdrop_sampler_(std::move(backdrop_sampler)),
      backdrop_sampler_descriptor_(backdrop_sampler_descriptor), metering_(std::move(metering)),
      timestamps_(std::move(timestamps)) {}

Drawing::~Drawing() = default;

std::expected<void, DeviceError> Drawing::set_map(Device& device, const Map& map,
                                                  const MapAssets& assets) {
    // Nothing reads the old map's textures now, since every batch of work is waited for.
    geometry_.reset();
    auto triangles = triangulate(map, assets.materials);

    std::vector<const TextureImage*> images;
    images.reserve(assets.images.size() + 1);
    for (const auto& image : assets.images) {
        images.push_back(&image);
    }
    images.push_back(&white_image());
    auto textures = upload_textures(device, images);
    if (!textures) {
        return std::unexpected(std::move(textures).error());
    }
    for (std::size_t index = 0; const auto& texture : *textures) {
        if (index < texture_descriptors_.size()) {
            device.descriptors().replace_sampled_image(texture_descriptors_.at(index),
                                                       texture.view());
        } else {
            texture_descriptors_.push_back(device.descriptors().add_sampled_image(texture.view()));
        }
        ++index;
    }
    // Places left over from a map with more images show the white one, so that none names an image
    // that is gone.
    for (const auto descriptor : texture_descriptors_ | std::views::drop(textures->size())) {
        device.descriptors().replace_sampled_image(descriptor, textures->back().view());
    }
    const auto white = texture_descriptors_.at(assets.images.size());

    // The materials, then each sky, which shades its backdrop's image at its own nits or else the
    // backdrop's.
    std::vector<MaterialShading> materials;
    materials.reserve(assets.materials.size() + triangles.skies.size());
    for (const auto& material : assets.materials) {
        materials.push_back({
            .base_color = material.color.factor,
            .color_texture =
                material.color.texture ? texture_descriptors_.at(*material.color.texture) : white,
        });
    }
    for (const auto& sky : triangles.skies) {
        const auto& backdrop = assets.backdrops.at(sky.backdrop.index());
        const auto nits = sky.nits.value_or(backdrop.nits);
        materials.push_back({
            .base_color = {.r = nits, .g = nits, .b = nits},
            .color_texture = texture_descriptors_.at(backdrop.image),
            .sky = 1,
        });
    }
    std::vector<SectorShading> sectors;
    sectors.reserve(map.sectors().size());
    for (const auto& sector : map.sectors()) {
        sectors.push_back({.ambient = sector.ambient.color * sector.ambient.lux});
    }

    auto vertex_buffer = upload<SectorVertex>(device, triangles.vertices);
    if (!vertex_buffer) {
        return std::unexpected(std::move(vertex_buffer).error());
    }
    auto material_buffer = upload<MaterialShading>(device, materials);
    if (!material_buffer) {
        return std::unexpected(std::move(material_buffer).error());
    }
    auto sector_buffer = upload<SectorShading>(device, sectors);
    if (!sector_buffer) {
        return std::unexpected(std::move(sector_buffer).error());
    }
    std::vector<SurfaceDraw> surfaces;
    surfaces.reserve(triangles.surfaces.size());
    for (const auto& surface : triangles.surfaces) {
        surfaces.push_back({
            .first_vertex = narrow<std::uint32_t>(surface.vertices.first),
            .vertex_count = narrow<std::uint32_t>(surface.vertices.count),
            .lowest = surface.lowest,
            .highest = surface.highest,
        });
    }
    auto surface_buffer = upload<SurfaceDraw>(device, surfaces);
    if (!surface_buffer) {
        return std::unexpected(std::move(surface_buffer).error());
    }
    // One view sees each sector once at most, and draws each of their surfaces once at most.
    constexpr VkBufferUsageFlags indirect_usage =
        shader_buffer_usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    auto views = buffer_of<ViewCulling>(device, 1);
    if (!views) {
        return std::unexpected(std::move(views).error());
    }
    auto sectors_in_view = buffer_of<SectorInView>(device, map.sectors().size());
    if (!sectors_in_view) {
        return std::unexpected(std::move(sectors_in_view).error());
    }
    auto draws = buffer_of<DrawCommand>(device, surfaces.size(), indirect_usage);
    if (!draws) {
        return std::unexpected(std::move(draws).error());
    }
    auto draw_counts = buffer_of<std::uint32_t>(device, 1, indirect_usage);
    if (!draw_counts) {
        return std::unexpected(std::move(draw_counts).error());
    }
    geometry_ = std::make_unique<Geometry>(
        &map, std::move(*vertex_buffer), std::move(*material_buffer), std::move(*sector_buffer),
        std::move(*surface_buffer), std::move(triangles.sectors), std::move(*views),
        std::move(*sectors_in_view), std::move(*draws), std::move(*draw_counts),
        std::move(*textures));
    metering_->exposure = map.exposure();
    metering_->last_frame.reset();
    return {};
}

std::expected<void, DeviceError> Drawing::fit_targets(Device& device, Extent extent) {
    if (targets_ && targets_->extent == extent) {
        return {};
    }
    // Nothing reads the old targets now, since every batch of work is waited for.
    targets_.reset();
    auto made = Targets::create(device, extent, samples_, scene_descriptor_);
    if (!made) {
        return std::unexpected(std::move(made).error());
    }
    targets_ = std::move(*made);
    return {};
}

float Drawing::next_blend() {
    const auto now = std::chrono::steady_clock::now();
    const auto last = std::exchange(metering_->last_frame, now);
    const auto speed = metering_->adaptation_speed;
    if (!last || std::isinf(speed)) {
        return 1.0F;
    }
    const std::chrono::duration<float> elapsed = now - *last;
    return 1.0F - std::exp(-speed * elapsed.count());
}

void Drawing::record_passes(const Device& device, VkCommandBuffer commands,
                            const PortalTraversal& traversal, float blend, VkPipeline resolve,
                            VkImageView output, VkQueryPool timestamps) const {
    const auto views = write_culling(traversal, geometry_->sector_surfaces, geometry_->views,
                                     geometry_->sectors_in_view);
    const CullingConstants culling_constants{
        .views = geometry_->views.address(),
        .sectors = geometry_->sectors_in_view.address(),
        .surfaces = geometry_->surfaces.address(),
        .draws = geometry_->draws.address(),
        .draw_counts = geometry_->draw_counts.address(),
    };
    const ResolveConstants resolve_constants{
        .scene = targets_->scene_descriptor,
        .ev100 = metering_->ev100.address(),
    };
    const auto size = vulkan_extent(targets_->extent);
    const bool meters = !metering_->pinned;
    constexpr BarrierScope metering_access{
        .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    };
    if (timestamps != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(commands, timestamps, 0, timestamp_count);
    }
    record_timestamp(commands, timestamps, 0);
    if (meters) {
        // The frame before may still be metering into the histogram and the exposure, or reading
        // the exposure in its AgX pass.
        record_memory_barrier(commands,
                              {
                                  .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                              },
                              metering_access);
    }

    device.record_binding(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_->cull.get(),
                          std::as_bytes(std::span(&culling_constants, 1)));
    vkCmdDispatch(commands, narrow<std::uint32_t>(traversal.views.size()), 1, 1);
    // The draws are read to draw, and their counts by the CPU too, once a measured frame is done.
    record_memory_barrier(
        commands,
        {
            .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        },
        {
            .stages = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
            .access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_HOST_READ_BIT,
        });
    record_timestamp(commands, timestamps, 1);

    record_image_barrier(commands, targets_->scene.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                         color_attachment_write);
    record_image_barrier(commands, targets_->depth.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, no_earlier_work,
                         depth_attachment_access, aspects_of(scene_depth_format));
    const VkRenderingAttachmentInfo scene_color{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = targets_->scene.view(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    // Depth is cleared to infinitely far away, and nothing after this pass reads it.
    const VkRenderingAttachmentInfo scene_depth{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = targets_->depth.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {.depth = 0.0F, .stencil = 0}},
    };
    const VkRenderingInfo scene_rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.extent = size},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &scene_color,
        .pDepthAttachment = &scene_depth,
        .pStencilAttachment = &scene_depth,
    };
    vkCmdBeginRendering(commands, &scene_rendering);
    set_viewport(commands, size);
    for (std::uint32_t index = 0; const auto& view : views) {
        const SceneConstants scene_constants{
            .view_projection = view.view_projection,
            .vertices = geometry_->vertices.address(),
            .materials = geometry_->materials.address(),
            .sectors = geometry_->sectors.address(),
            .texture_sampler = sampler_descriptor_,
            .eye = traversal.views.at(index).eye,
            .backdrop_sampler = backdrop_sampler_descriptor_,
        };
        device.record_binding(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_->scene.get(),
                              std::as_bytes(std::span(&scene_constants, 1)));
        vkCmdDrawIndirectCount(commands, geometry_->draws.get(),
                               view.first_draw * sizeof(DrawCommand), geometry_->draw_counts.get(),
                               index * sizeof(std::uint32_t), view.candidate_count,
                               sizeof(DrawCommand));
        ++index;
    }
    vkCmdEndRendering(commands);
    record_timestamp(commands, timestamps, 2);

    record_image_barrier(commands, targets_->scene.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, color_attachment_write,
                         {
                             .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                       VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                             .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                         });
    if (meters) {
        const MeteringConstants metering_constants{
            .scene = targets_->scene_descriptor,
            .histogram = metering_->histogram.address(),
            .ev100 = metering_->ev100.address(),
            .extent = {size.width, size.height},
            .min_ev100 = metering_->exposure.min,
            .max_ev100 = metering_->exposure.max,
            .compensation = metering_->exposure.compensation,
            .blend = blend,
        };
        const auto metering_bytes = std::as_bytes(std::span(&metering_constants, 1));
        device.record_binding(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_->gather.get(),
                              metering_bytes);
        vkCmdDispatch(commands, (size.width + histogram_tile - 1) / histogram_tile,
                      (size.height + histogram_tile - 1) / histogram_tile, 1);
        record_memory_barrier(commands, metering_access, metering_access);
        device.record_binding(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_->meter.get(),
                              metering_bytes);
        vkCmdDispatch(commands, 1, 1, 1);
        record_memory_barrier(commands, metering_access,
                              {
                                  .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  .access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                              });
    }
    record_timestamp(commands, timestamps, 3);
    const VkRenderingAttachmentInfo output_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = output,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo output_rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.extent = size},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &output_attachment,
    };
    vkCmdBeginRendering(commands, &output_rendering);
    device.record_binding(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, resolve,
                          std::as_bytes(std::span(&resolve_constants, 1)));
    set_viewport(commands, size);
    vkCmdDraw(commands, 3, 1, 0, 0);
    vkCmdEndRendering(commands);
    record_timestamp(commands, timestamps, 4);
}

std::expected<Image, DeviceError> Drawing::render_image(Device& device, const Camera& camera,
                                                        Extent extent) {
    if (auto fitted = fit_targets(device, extent); !fitted) {
        return std::unexpected(std::move(fitted).error());
    }
    const auto size = vulkan_extent(extent);
    const auto blend = next_blend();
    const auto traversal = traverse_portals(*geometry_->map, camera, extent);
    const auto executed = device.execute([&](VkCommandBuffer commands) {
        record_image_barrier(commands, targets_->output.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                             color_attachment_write);
        record_passes(device, commands, traversal, blend, pipelines_->resolve.get(),
                      targets_->output.view());

        record_image_barrier(
            commands, targets_->output.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, color_attachment_write,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_READ_BIT});
        const VkBufferImageCopy copy{
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
            .imageExtent = {.width = size.width, .height = size.height, .depth = 1},
        };
        vkCmdCopyImageToBuffer(commands, targets_->output.get(),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targets_->readback.get(), 1,
                               &copy);
        record_memory_barrier(
            commands,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
            {.stages = VK_PIPELINE_STAGE_2_HOST_BIT, .access = VK_ACCESS_2_HOST_READ_BIT});
    });
    if (!executed) {
        return std::unexpected(executed.error());
    }
    const auto memory = targets_->readback.memory();
    std::vector<std::uint8_t> pixels(memory.size());
    std::memcpy(pixels.data(), memory.data(), memory.size());
    return Image(extent, std::move(pixels));
}

std::expected<FrameMeasurement, DeviceError>
Drawing::measure_frame(Device& device, const Camera& camera, Extent extent) {
    const auto start = std::chrono::steady_clock::now();
    if (auto fitted = fit_targets(device, extent); !fitted) {
        return std::unexpected(std::move(fitted).error());
    }
    const auto blend = next_blend();
    const auto traversal = traverse_portals(*geometry_->map, camera, extent);
    const auto submitted = device.submit([&](VkCommandBuffer commands) {
        record_image_barrier(commands, targets_->output.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                             color_attachment_write);
        record_passes(device, commands, traversal, blend, pipelines_->resolve.get(),
                      targets_->output.view(), timestamps_.get());
    });
    if (!submitted) {
        return std::unexpected(submitted.error());
    }
    const auto submission = std::chrono::steady_clock::now() - start;
    if (auto waited = device.wait(); !waited) {
        return std::unexpected(waited.error());
    }

    std::array<std::uint64_t, timestamp_count> ticks{};
    if (const auto error =
            failure(vkGetQueryPoolResults(device.get(), timestamps_.get(), 0, timestamp_count,
                                          sizeof(ticks), ticks.data(), sizeof(std::uint64_t),
                                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
                    "reading the timestamps that measure a frame")) {
        return std::unexpected(*error);
    }
    FrameMeasurement measurement{
        .submission = std::chrono::duration_cast<std::chrono::nanoseconds>(submission),
        .views = traversal.views.size(),
        .device_memory = device.allocated_bytes(),
    };
    std::vector<std::uint32_t> draw_counts(traversal.views.size());
    std::memcpy(draw_counts.data(), geometry_->draw_counts.memory().data(),
                std::span(draw_counts).size_bytes());
    for (const auto count : draw_counts) {
        measurement.draws += count;
    }
    measurement.passes.reserve(timed_passes.size());
    // A timestamp counts in steps of the device's period, in as many bits as the queue writes, so
    // each pass is the steps between two of them, wrapping around.
    for (const auto [name, before, after] :
         std::views::zip(timed_passes, ticks, ticks | std::views::drop(1))) {
        const auto steps = (after - before) & device.timestamp_mask();
        const std::chrono::nanoseconds time{std::llround(
            static_cast<double>(steps) * static_cast<double>(device.timestamp_period()))};
        measurement.passes.push_back({.name = name, .time = time});
        measurement.gpu += time;
    }
    return measurement;
}

std::expected<void, DeviceError> Drawing::draw(Device& device, const Camera& camera,
                                               const Frame& frame) {
    if (auto fitted = fit_targets(device, frame.extent); !fitted) {
        return fitted;
    }
    const auto blend = next_blend();
    const auto traversal = traverse_portals(*geometry_->map, camera, frame.extent);
    return device.execute(
        [&](VkCommandBuffer commands) {
            // The image is written once the semaphore that the submission waits for at this stage
            // says it is acquired.
            record_image_barrier(commands, frame.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 {.stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                                 color_attachment_write);
            record_passes(device, commands, traversal, blend, pipelines_->present.get(),
                          frame.view);
            // The semaphore that the submission signals, after every command, makes the image's
            // new layout visible to the present that waits for it.
            record_image_barrier(commands, frame.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_attachment_write,
                                 no_later_work);
        },
        {.wait = frame.acquired, .signal = frame.rendered});
}

} // namespace wulfhilde::detail
