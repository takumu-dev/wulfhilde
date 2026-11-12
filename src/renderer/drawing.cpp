#include "drawing.hpp"

#include "barriers.hpp"
#include "bindless_descriptors.hpp"
#include "device.hpp"
#include "formats.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "pipelines.hpp"
#include "sector_geometry.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/core/angle.hpp>
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
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <optional>
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

} // namespace

struct Drawing::Pipelines {
    PipelineHandle scene;
    PipelineHandle resolve;
};

struct Drawing::Geometry {
    Buffer vertices;
    Buffer materials;
    Buffer sectors;
    std::vector<VertexRange> ranges;
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

std::expected<std::unique_ptr<Drawing>, DeviceError>
Drawing::create(Device& device, VkSampleCountFlagBits samples) {
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
    // A scene of one sample a pixel isn't a multisampled image, so it is read another way.
    const std::array<std::uint32_t, 1> sample_count{static_cast<std::uint32_t>(samples)};
    auto resolve = create_graphics_pipeline(
        device, {
                    .spirv = resolve_spirv,
                    .vertex_entry_point = "cover",
                    .fragment_entry_point =
                        samples == VK_SAMPLE_COUNT_1_BIT ? "resolve_sample" : "resolve_samples",
                    .specialization_constants = sample_count,
                    .color_format = output_format,
                });
    if (!resolve) {
        return std::unexpected(std::move(resolve).error());
    }
    return std::make_unique<Drawing>(
        Key{}, samples, std::make_unique<Pipelines>(std::move(*scene), std::move(*resolve)));
}

Drawing::Drawing(Key /*key*/, VkSampleCountFlagBits samples,
                 std::unique_ptr<Pipelines> pipelines) noexcept
    : samples_(samples), pipelines_(std::move(pipelines)) {}

Drawing::~Drawing() = default;

std::expected<void, DeviceError> Drawing::set_map(const Device& device, const Map& map,
                                                  const MapAssets& assets) {
    geometry_.reset();
    auto triangles = triangulate(map);

    std::vector<MaterialShading> materials;
    materials.reserve(assets.materials.size());
    for (const auto& material : assets.materials) {
        materials.push_back({.base_color = material.color.factor});
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
                                           std::move(*sector_buffer), std::move(triangles.sectors));
    return {};
}

std::expected<Image, DeviceError> Drawing::render_image(Device& device, const Camera& camera,
                                                        Extent extent) {
    if (!targets_ || targets_->extent != extent) {
        // Nothing reads the old targets now, since every batch of work is waited for.
        targets_.reset();
        auto made = Targets::create(device, extent, samples_, scene_descriptor_);
        if (!made) {
            return std::unexpected(std::move(made).error());
        }
        targets_ = std::move(*made);
    }

    const SceneConstants scene_constants{
        .view_projection = view_projection(camera, extent),
        .vertices = geometry_->vertices.address(),
        .materials = geometry_->materials.address(),
        .sectors = geometry_->sectors.address(),
    };
    const ResolveConstants resolve_constants{
        .scene = targets_->scene_descriptor,
        .exposure = exposure_of(pinned_exposure_ev100),
    };

    const auto size = vulkan_extent(extent);
    const auto executed = device.execute([&](VkCommandBuffer commands) {
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

        record_image_barrier(commands, targets_->scene.get(),
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, color_attachment_write,
                             {
                                 .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                 .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                             });
        record_image_barrier(commands, targets_->output.get(), VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                             color_attachment_write);
        const VkRenderingAttachmentInfo output{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = targets_->output.view(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        const VkRenderingInfo output_rendering{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = size},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &output,
        };
        vkCmdBeginRendering(commands, &output_rendering);
        device.record_binding(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_->resolve.get(),
                              std::as_bytes(std::span(&resolve_constants, 1)));
        set_viewport(commands, size);
        vkCmdDraw(commands, 3, 1, 0, 0);
        vkCmdEndRendering(commands);

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

} // namespace wulfhilde::detail
