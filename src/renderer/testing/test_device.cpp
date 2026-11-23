#include <wulfhilde/renderer/testing/test_device.hpp>

#include "barriers.hpp"
#include "device.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "pipelines.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

constexpr VkFormat color_format = VK_FORMAT_R8G8B8A8_SRGB;

constexpr VkBufferUsageFlags table_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

VkExtent2D vulkan_extent(Extent extent) {
    return {
        .width = narrow<std::uint32_t>(extent.width),
        .height = narrow<std::uint32_t>(extent.height),
    };
}

std::size_t byte_count(Extent extent) {
    return extent.width * extent.height * Image::bytes_per_pixel;
}

VkBufferImageCopy whole_image_copy(Extent extent) {
    const auto size = vulkan_extent(extent);
    return {
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
        .imageExtent = {.width = size.width, .height = size.height, .depth = 1},
    };
}

} // namespace

struct TestDevice::BindlessContents {
    std::vector<GpuImage> textures;
    std::vector<SamplerHandle> samplers;
};

std::expected<TestDevice, DeviceError> TestDevice::create(RendererSettings settings,
                                                          std::uint32_t newest_minor_version) {
    auto device =
        Device::create(settings, nullptr, VK_MAKE_API_VERSION(0, 1, newest_minor_version, 0));
    if (!device) {
        return std::unexpected(std::move(device).error());
    }
    return TestDevice(std::move(*device));
}

TestDevice::TestDevice(std::unique_ptr<Device> device)
    : device_(std::move(device)), contents_(std::make_unique<BindlessContents>()) {}

TestDevice::TestDevice(TestDevice&& other) noexcept = default;

TestDevice& TestDevice::operator=(TestDevice&& other) noexcept {
    if (this != &other) {
        // The textures and samplers go before the device they were made on.
        contents_ = std::move(other.contents_);
        device_ = std::move(other.device_);
    }
    return *this;
}

TestDevice::~TestDevice() {
    // The textures and samplers go before the device they were made on, which waits for its work
    // first.
    if (device_) {
        static_cast<void>(vkDeviceWaitIdle(device_->get()));
    }
    contents_.reset();
}

std::expected<std::vector<std::byte>, DeviceError>
TestDevice::run_table(std::span<const std::uint32_t> spirv, std::string_view entry_point,
                      std::span<const std::byte> inputs, std::size_t row_count,
                      std::size_t result_size) {
    WULFHILDE_ASSERT(row_count > 0, "a table has at least one row");
    WULFHILDE_ASSERT(!inputs.empty() && result_size > 0, "a table has inputs and results");
    const auto pipeline = create_compute_pipeline(*device_, spirv, entry_point);
    if (!pipeline) {
        return std::unexpected(pipeline.error());
    }
    auto input_buffer =
        Buffer::create(device_->allocator(), device_->get(), inputs.size(), table_usage);
    if (!input_buffer) {
        return std::unexpected(std::move(input_buffer).error());
    }
    std::ranges::copy(inputs, input_buffer->memory().begin());
    auto result_buffer =
        Buffer::create(device_->allocator(), device_->get(), result_size, table_usage);
    if (!result_buffer) {
        return std::unexpected(std::move(result_buffer).error());
    }

    const TableConstants constants{
        .row_count = narrow<std::uint32_t>(row_count),
        .inputs = input_buffer->address(),
        .results = result_buffer->address(),
    };
    const auto group_count =
        narrow<std::uint32_t>((row_count + table_group_size - 1) / table_group_size);
    const auto executed = device_->execute([&](VkCommandBuffer commands) {
        device_->record_binding(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->get(),
                                std::as_bytes(std::span(&constants, 1)));
        vkCmdDispatch(commands, group_count, 1, 1);
        record_memory_barrier(
            commands,
            {
                .stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            },
            {.stages = VK_PIPELINE_STAGE_2_HOST_BIT, .access = VK_ACCESS_2_HOST_READ_BIT});
    });
    if (!executed) {
        return std::unexpected(executed.error());
    }
    const auto results = result_buffer->memory();
    return std::vector<std::byte>(results.begin(), results.end());
}

std::expected<std::uint32_t, DeviceError> TestDevice::add_texture(const Image& image) {
    auto staging = Buffer::create(device_->allocator(), device_->get(), image.pixels().size(),
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!staging) {
        return std::unexpected(std::move(staging).error());
    }
    std::memcpy(staging->memory().data(), image.pixels().data(), image.pixels().size());
    auto texture =
        GpuImage::create(device_->allocator(), device_->get(), color_format, image.extent(),
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    if (!texture) {
        return std::unexpected(std::move(texture).error());
    }

    const auto executed = device_->execute([&](VkCommandBuffer commands) {
        record_image_barrier(
            commands, texture->get(), VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, no_earlier_work,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
        const auto copy = whole_image_copy(image.extent());
        vkCmdCopyBufferToImage(commands, staging->get(), texture->get(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        record_image_barrier(
            commands, texture->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
            {
                .stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            });
    });
    if (!executed) {
        return std::unexpected(executed.error());
    }
    const auto handle = device_->descriptors().add_sampled_image(texture->view());
    contents_->textures.push_back(std::move(*texture));
    return handle.index;
}

std::expected<std::uint32_t, DeviceError> TestDevice::add_nearest_sampler() {
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateSampler(device_->get(), &info, nullptr, &sampler),
                                   "creating a sampler")) {
        return std::unexpected(*error);
    }
    contents_->samplers.emplace_back(device_->get(), sampler);
    return device_->descriptors().add_sampler(sampler).index;
}

void TestDevice::break_valid_usage() {
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 1024.0F,
    };
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device_->get(), &info, nullptr, &sampler) == VK_SUCCESS) {
        vkDestroySampler(device_->get(), sampler, nullptr);
    }
}

std::expected<Image, DeviceError> TestDevice::draw(const TestDrawing& drawing) {
    WULFHILDE_ASSERT(drawing.vertex_count % 3 == 0,
                     "a list of triangles has three vertices for each");
    const auto pipeline = create_graphics_pipeline(
        *device_, {
                      .spirv = drawing.spirv,
                      .vertex_entry_point = drawing.vertex_entry_point,
                      .fragment_entry_point = drawing.fragment_entry_point,
                      .specialization_constants = drawing.specialization_constants,
                      .color_format = color_format,
                  });
    if (!pipeline) {
        return std::unexpected(pipeline.error());
    }
    auto target =
        GpuImage::create(device_->allocator(), device_->get(), color_format, drawing.extent,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!target) {
        return std::unexpected(std::move(target).error());
    }
    auto readback = Buffer::create(device_->allocator(), device_->get(), byte_count(drawing.extent),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (!readback) {
        return std::unexpected(std::move(readback).error());
    }

    const auto size = vulkan_extent(drawing.extent);
    const auto executed = device_->execute([&](VkCommandBuffer commands) {
        constexpr BarrierScope attachment_write{
            .stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        };
        record_image_barrier(commands, target->get(), VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, no_earlier_work,
                             attachment_write);
        const VkRenderingAttachmentInfo attachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = target->view(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        const VkRenderingInfo rendering{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = size},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachment,
        };
        vkCmdBeginRendering(commands, &rendering);
        device_->record_binding(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get(),
                                drawing.push_constants);
        const VkViewport viewport{
            .width = static_cast<float>(size.width),
            .height = static_cast<float>(size.height),
            .maxDepth = 1.0F,
        };
        const VkRect2D scissor{.extent = size};
        vkCmdSetViewport(commands, 0, 1, &viewport);
        vkCmdSetScissor(commands, 0, 1, &scissor);
        vkCmdDraw(commands, narrow<std::uint32_t>(drawing.vertex_count), 1, 0, 0);
        vkCmdEndRendering(commands);

        record_image_barrier(
            commands, target->get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, attachment_write,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_READ_BIT});
        const auto copy = whole_image_copy(drawing.extent);
        vkCmdCopyImageToBuffer(commands, target->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback->get(), 1, &copy);
        record_memory_barrier(
            commands,
            {.stages = VK_PIPELINE_STAGE_2_COPY_BIT, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
            {.stages = VK_PIPELINE_STAGE_2_HOST_BIT, .access = VK_ACCESS_2_HOST_READ_BIT});
    });
    if (!executed) {
        return std::unexpected(executed.error());
    }
    const auto memory = readback->memory();
    std::vector<std::uint8_t> pixels(memory.size());
    std::memcpy(pixels.data(), memory.data(), memory.size());
    return Image(drawing.extent, std::move(pixels));
}

} // namespace wulfhilde::detail
