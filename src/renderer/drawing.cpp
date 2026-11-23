#include "drawing.hpp"

#include "barriers.hpp"
#include "bindless_descriptors.hpp"
#include "device.hpp"
#include "formats.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "pipelines.hpp"
#include "presentation.hpp"
#include "sector_geometry.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector4.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/world/map.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

#include "resolve_spirv.h"
#include "sectors_spirv.h"

// The C++ twin of the sector shaders' `MaterialShading`.
struct MaterialShading {
    Color base_color;
    DescriptorHandle color_texture;
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
};

// The C++ twin of the resolve shaders' push constants.
struct ResolveConstants {
    DescriptorHandle scene;
    float exposure = 0.0F;
};

#include "resolve_layout.hpp" // IWYU pragma: keep
#include "sectors_layout.hpp" // IWYU pragma: keep

// How near the eye a surface can be and still be drawn, in metres. Depth is reversed, so a fixed
// near plane loses no precision far away, and there is no far plane at all.
constexpr float near_plane = 0.05F;

// The exposure, in EV at ISO 100, until it is worked out from what the camera sees. A mid-grey
// surface under 250 lux of ambient light shows as a mid-grey.
constexpr float pinned_exposure_ev100 = 6.0F;

// What luminance in nits is multiplied by for AgX, which takes 0.18 as mid-grey, at an exposure:
// the luminance that just saturates a camera's sensor, by the usual calibration of 12.5 and a
// sensor saturating at 1.2 times its nominal response, maps to 1.
float exposure_of(float ev100) { return 1.0F / (1.2F * std::exp2(ev100)); }

constexpr VkBufferUsageFlags shader_buffer_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

// From a camera's sector's coordinates into Vulkan's clip space: x to the right, y down, and depth
// from 1 at the near plane to 0 infinitely far away.
Matrix4 view_projection(const Camera& camera, Extent extent) {
    // Into the eye's own axes, as the camera's orientation has them: forward, left and up.
    const auto view = to_matrix(inverse(RigidTransform{
        .rotation = camera.orientation,
        .translation = camera.location.point,
    }));
    const auto vertical =
        1.0F / tan(Angle::radians(camera.vertical_field_of_view.in_radians() / 2.0F));
    const auto horizontal =
        vertical * static_cast<float>(extent.height) / static_cast<float>(extent.width);
    // Left is to the left and up is up, so each turns negative in the image, and depth is the near
    // plane over the distance forward, which divides through as w.
    const Matrix4 projection{
        .columns =
            {
                Vector4{.w = 1.0F},
                Vector4{.x = -horizontal},
                Vector4{.y = -vertical},
                Vector4{.z = near_plane},
            },
    };
    return projection * view;
}

VkExtent2D vulkan_extent(Extent extent) {
    return {
        .width = narrow<std::uint32_t>(extent.width),
        .height = narrow<std::uint32_t>(extent.height),
    };
}

template <typename T>
std::expected<Buffer, DeviceError> upload(const Device& device, std::span<const T> values) {
    auto buffer =
        Buffer::create(device.allocator(), device.get(), values.size_bytes(), shader_buffer_usage);
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

// Makes the sampler that reads every texture: repeating, filtered trilinearly between mip levels,
// and anisotropically with up to the given number of samples, where more than one.
std::expected<SamplerHandle, DeviceError> create_texture_sampler(const Device& device,
                                                                 std::size_t anisotropy) {
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = anisotropy > 1 ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = static_cast<float>(anisotropy),
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateSampler(device.get(), &info, nullptr, &sampler),
                                   "creating the textures' sampler")) {
        return std::unexpected(*error);
    }
    return SamplerHandle(device.get(), sampler);
}

} // namespace

struct Drawing::Pipelines {
    PipelineHandle scene;
    PipelineHandle resolve;
    // The AgX pass into a window's images, or none without a window.
    PipelineHandle present;
};

struct Drawing::Geometry {
    Buffer vertices;
    Buffer materials;
    Buffer sectors;
    std::vector<VertexRange> ranges;
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
Drawing::create(Device& device, VkSampleCountFlagBits samples, std::size_t anisotropy,
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
    auto sampler = create_texture_sampler(device, anisotropy);
    if (!sampler) {
        return std::unexpected(std::move(sampler).error());
    }
    const auto sampler_descriptor = device.descriptors().add_sampler(sampler->get());
    return std::make_unique<Drawing>(
        Key{}, samples,
        std::make_unique<Pipelines>(std::move(*scene), std::move(*resolve), std::move(present)),
        std::move(*sampler), sampler_descriptor);
}

Drawing::Drawing(Key /*key*/, VkSampleCountFlagBits samples, std::unique_ptr<Pipelines> pipelines,
                 SamplerHandle texture_sampler, DescriptorHandle sampler_descriptor) noexcept
    : samples_(samples), pipelines_(std::move(pipelines)),
      texture_sampler_(std::move(texture_sampler)), sampler_descriptor_(sampler_descriptor) {}

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

    std::vector<MaterialShading> materials;
    materials.reserve(assets.materials.size());
    for (const auto& material : assets.materials) {
        materials.push_back({
            .base_color = material.color.factor,
            .color_texture =
                material.color.texture ? texture_descriptors_.at(*material.color.texture) : white,
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
    geometry_ = std::make_unique<Geometry>(std::move(*vertex_buffer), std::move(*material_buffer),
                                           std::move(*sector_buffer), std::move(triangles.sectors),
                                           std::move(*textures));
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

void Drawing::record_passes(const Device& device, VkCommandBuffer commands, const Camera& camera,
                            VkPipeline resolve, VkImageView output) const {
    const SceneConstants scene_constants{
        .view_projection = view_projection(camera, targets_->extent),
        .vertices = geometry_->vertices.address(),
        .materials = geometry_->materials.address(),
        .sectors = geometry_->sectors.address(),
        .texture_sampler = sampler_descriptor_,
    };
    const ResolveConstants resolve_constants{
        .scene = targets_->scene_descriptor,
        .exposure = exposure_of(pinned_exposure_ev100),
    };
    const auto size = vulkan_extent(targets_->extent);

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
    device.record_binding(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_->scene.get(),
                          std::as_bytes(std::span(&scene_constants, 1)));
    set_viewport(commands, size);
    for (const auto& range : geometry_->ranges) {
        vkCmdDraw(commands, narrow<std::uint32_t>(range.count), 1,
                  narrow<std::uint32_t>(range.first), 0);
    }
    vkCmdEndRendering(commands);

    record_image_barrier(commands, targets_->scene.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, color_attachment_write,
                         {
                             .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                             .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                         });
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
}

std::expected<Image, DeviceError> Drawing::render_image(Device& device, const Camera& camera,
                                                        Extent extent) {
    if (auto fitted = fit_targets(device, extent); !fitted) {
        return std::unexpected(std::move(fitted).error());
    }
    const auto size = vulkan_extent(extent);
    const auto executed = device.execute([&](VkCommandBuffer commands) {
        record_image_barrier(commands, targets_->output.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                             color_attachment_write);
        record_passes(device, commands, camera, pipelines_->resolve.get(), targets_->output.view());

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

std::expected<void, DeviceError> Drawing::draw(Device& device, const Camera& camera,
                                               const Frame& frame) {
    if (auto fitted = fit_targets(device, frame.extent); !fitted) {
        return fitted;
    }
    return device.execute(
        [&](VkCommandBuffer commands) {
            // The image is written once the semaphore that the submission waits for at this stage
            // says it is acquired.
            record_image_barrier(commands, frame.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 {.stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT},
                                 color_attachment_write);
            record_passes(device, commands, camera, pipelines_->present.get(), frame.view);
            // The semaphore that the submission signals, after every command, makes the image's
            // new layout visible to the present that waits for it.
            record_image_barrier(commands, frame.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_attachment_write,
                                 no_later_work);
        },
        {.wait = frame.acquired, .signal = frame.rendered});
}

} // namespace wulfhilde::detail
