#include "pipelines.hpp"

#include "device.hpp"
#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

// The stages of one pipeline, all from one SPIR-V module: given to the pipeline directly where the
// device allows it, and otherwise through a shader module that lives as long as this does. Each
// stage sees the same values of the module's specialisation constants, which stand in for the
// variants a preprocessor would otherwise make.
class ShaderStages {
  public:
    static std::expected<ShaderStages, DeviceError>
    create(const Device& device, std::span<const std::uint32_t> spirv,
           std::span<const std::pair<VkShaderStageFlagBits, std::string_view>> entry_points,
           std::span<const std::uint32_t> specialization_constants) {
        WULFHILDE_ASSERT(!spirv.empty(), "a SPIR-V module holds at least its header");
        ShaderStages stages;
        stages.module_info_ = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size_bytes(),
            .pCode = spirv.data(),
        };
        if (!device.takes_spirv_directly()) {
            VkShaderModule made = VK_NULL_HANDLE;
            if (const auto error = failure(
                    vkCreateShaderModule(device.get(), &stages.module_info_, nullptr, &made),
                    "creating a shader module")) {
                return std::unexpected(*error);
            }
            stages.module_ = ShaderModuleHandle(device.get(), made);
        }
        for (const auto& [stage, name] : entry_points) {
            stages.entry_points_.emplace_back(stage, name);
        }
        stages.constants_.assign(specialization_constants.begin(), specialization_constants.end());
        for (std::size_t id = 0; id < stages.constants_.size(); ++id) {
            stages.constant_entries_.push_back({
                .constantID = narrow<std::uint32_t>(id),
                .offset = narrow<std::uint32_t>(id * sizeof(std::uint32_t)),
                .size = sizeof(std::uint32_t),
            });
        }
        return stages;
    }

    // The stages, which point into this, so it must stay where it is while they are used.
    [[nodiscard]] std::vector<VkPipelineShaderStageCreateInfo> infos() {
        specialization_ = {
            .mapEntryCount = narrow<std::uint32_t>(constant_entries_.size()),
            .pMapEntries = constant_entries_.data(),
            .dataSize = constants_.size() * sizeof(std::uint32_t),
            .pData = constants_.data(),
        };
        std::vector<VkPipelineShaderStageCreateInfo> infos;
        infos.reserve(entry_points_.size());
        for (const auto& [stage, name] : entry_points_) {
            infos.push_back({
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = module_.get() == VK_NULL_HANDLE ? &module_info_ : nullptr,
                .stage = stage,
                .module = module_.get(),
                .pName = name.c_str(),
                .pSpecializationInfo = constants_.empty() ? nullptr : &specialization_,
            });
        }
        return infos;
    }

  private:
    VkShaderModuleCreateInfo module_info_{};
    ShaderModuleHandle module_;
    std::vector<std::pair<VkShaderStageFlagBits, std::string>> entry_points_;
    std::vector<std::uint32_t> constants_;
    std::vector<VkSpecializationMapEntry> constant_entries_;
    VkSpecializationInfo specialization_{};
};

} // namespace

std::expected<PipelineHandle, DeviceError>
create_compute_pipeline(const Device& device, std::span<const std::uint32_t> spirv,
                        std::string_view entry_point,
                        std::span<const std::uint32_t> specialization_constants) {
    const std::array entry_points{std::pair{VK_SHADER_STAGE_COMPUTE_BIT, entry_point}};
    auto stages = ShaderStages::create(device, spirv, entry_points, specialization_constants);
    if (!stages) {
        return std::unexpected(stages.error());
    }
    const auto infos = stages->infos();
    const VkComputePipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = infos.front(),
        .layout = device.pipeline_layout(),
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (const auto error = failure(
            vkCreateComputePipelines(device.get(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
            "creating a compute pipeline")) {
        return std::unexpected(*error);
    }
    return PipelineHandle(device.get(), pipeline);
}

std::expected<PipelineHandle, DeviceError>
create_graphics_pipeline(const Device& device, const GraphicsPipelineDescription& description) {
    const std::array entry_points{
        std::pair{VK_SHADER_STAGE_VERTEX_BIT, description.vertex_entry_point},
        std::pair{VK_SHADER_STAGE_FRAGMENT_BIT, description.fragment_entry_point},
    };
    auto stages = ShaderStages::create(device, description.spirv, entry_points,
                                       description.specialization_constants);
    if (!stages) {
        return std::unexpected(stages.error());
    }
    const auto infos = stages->infos();

    const bool has_depth = description.depth_stencil_format != VK_FORMAT_UNDEFINED;
    const VkPipelineRenderingCreateInfo rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &description.color_format,
        .depthAttachmentFormat = description.depth_stencil_format,
        .stencilAttachmentFormat = description.depth_stencil_format,
    };
    const VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    const VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkPipelineViewportStateCreateInfo viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = description.cull_mode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0F,
    };
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = description.samples,
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = has_depth ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = has_depth ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_GREATER,
    };
    const VkPipelineColorBlendAttachmentState attachment{
        .colorWriteMask = flags_of(VK_COLOR_COMPONENT_R_BIT, VK_COLOR_COMPONENT_G_BIT,
                                   VK_COLOR_COMPONENT_B_BIT, VK_COLOR_COMPONENT_A_BIT),
    };
    const VkPipelineColorBlendStateCreateInfo blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
    };
    const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = narrow<std::uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
    const VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = narrow<std::uint32_t>(infos.size()),
        .pStages = infos.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = device.pipeline_layout(),
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (const auto error = failure(
            vkCreateGraphicsPipelines(device.get(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
            "creating a graphics pipeline")) {
        return std::unexpected(*error);
    }
    return PipelineHandle(device.get(), pipeline);
}

} // namespace wulfhilde::detail
