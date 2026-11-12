#include <wulfhilde/renderer/renderer.hpp>

#include "device.hpp"
#include "drawing.hpp"
#include "presentation.hpp"
#include "vulkan.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/map.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde {

namespace {

// Every sample count that Vulkan can name.
constexpr std::array<std::size_t, 7> named_sample_counts{1, 2, 4, 8, 16, 32, 64};

// The flag bit of a sample count, if it is one that Vulkan can name.
std::optional<VkSampleCountFlagBits> sample_count_bit(std::size_t count) {
    if (!std::ranges::contains(named_sample_counts, count)) {
        return std::nullopt;
    }
    return static_cast<VkSampleCountFlagBits>(count);
}

// The error of a sample count the device can't take, naming those it can: "only 1, 4 or 8".
DeviceError unsupported_sample_count(std::size_t count, VkSampleCountFlags supported) {
    const auto counts =
        named_sample_counts |
        std::views::filter([supported](std::size_t named) { return (supported & named) != 0; }) |
        std::ranges::to<std::vector>();
    std::string message = std::format("the device can't take {} samples a pixel, only", count);
    for (std::size_t index = 0; const auto named : counts) {
        std::string_view separator = ", ";
        if (index == 0) {
            separator = " ";
        } else if (index + 1 == counts.size()) {
            separator = " or ";
        }
        message += std::format("{}{}", separator, named);
        ++index;
    }
    return {.code = DeviceErrorCode::unsupported_sample_count, .message = std::move(message)};
}

} // namespace

std::expected<Renderer, DeviceError> Renderer::create(Window& window, RendererSettings settings) {
    return create_for(&window, settings);
}

std::expected<Renderer, DeviceError> Renderer::create_headless(RendererSettings settings) {
    return create_for(nullptr, settings);
}

std::expected<Renderer, DeviceError> Renderer::create_for(Window* window,
                                                          RendererSettings settings) {
    auto device = detail::Device::create(settings, window);
    if (!device) {
        return std::unexpected(std::move(device).error());
    }
    const auto samples = sample_count_bit(settings.sample_count);
    if (!samples || ((*device)->sample_counts() & detail::flags_of(*samples)) == 0) {
        return std::unexpected(
            unsupported_sample_count(settings.sample_count, (*device)->sample_counts()));
    }
    std::unique_ptr<detail::Presentation> presentation;
    std::optional<detail::WindowFormat> window_format;
    if (window != nullptr) {
        auto made = detail::Presentation::create(**device, *window);
        if (!made) {
            return std::unexpected(std::move(made).error());
        }
        presentation = std::move(*made);
        window_format = presentation->format();
    }
    auto drawing = detail::Drawing::create(**device, *samples, window_format);
    if (!drawing) {
        return std::unexpected(std::move(drawing).error());
    }
    return Renderer(std::move(*device), std::move(*drawing), std::move(presentation));
}

Renderer::Renderer(std::unique_ptr<detail::Device> device, std::unique_ptr<detail::Drawing> drawing,
                   std::unique_ptr<detail::Presentation> presentation) noexcept
    : device_(std::move(device)), drawing_(std::move(drawing)),
      presentation_(std::move(presentation)) {}

Renderer::Renderer(Renderer&& other) noexcept = default;

Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        // What is drawn with and shown with goes before the device it was made on.
        presentation_ = std::move(other.presentation_);
        drawing_ = std::move(other.drawing_);
        device_ = std::move(other.device_);
        map_ = std::exchange(other.map_, nullptr);
    }
    return *this;
}

Renderer::~Renderer() {
    // What is drawn with and shown with goes before the device it was made on, once the device is
    // idle.
    if (device_) {
        static_cast<void>(vkDeviceWaitIdle(device_->get()));
    }
    presentation_.reset();
    drawing_.reset();
}

std::expected<void, DeviceError> Renderer::set_map(const Map& map, const MapAssets& assets) {
    WULFHILDE_ASSERT(assets.materials.size() == map.materials().size(),
                     "the assets hold a material for each of the map's");
    map_ = nullptr;
    if (auto set = drawing_->set_map(*device_, map, assets); !set) {
        return set;
    }
    map_ = &map;
    return {};
}

std::expected<void, DeviceError> Renderer::draw(const Camera& camera) {
    WULFHILDE_ASSERT(presentation_ != nullptr, "a renderer draws frames only into a window");
    WULFHILDE_ASSERT(map_ != nullptr, "a map is set before a frame of it is drawn");
    WULFHILDE_ASSERT(camera.location.sector.index() < map_->sectors().size(),
                     "the camera is in a sector of the map that is set");
    const auto frame = presentation_->begin_frame(*device_);
    if (!frame) {
        return std::unexpected(frame.error());
    }
    if (!*frame) {
        return {};
    }
    if (auto drawn = drawing_->draw(*device_, camera, **frame); !drawn) {
        return drawn;
    }
    return presentation_->end_frame(*device_, **frame);
}

std::expected<Image, DeviceError> Renderer::render_image(const Camera& camera, Extent extent) {
    WULFHILDE_ASSERT(map_ != nullptr, "a map is set before an image of it is rendered");
    WULFHILDE_ASSERT(camera.location.sector.index() < map_->sectors().size(),
                     "the camera is in a sector of the map that is set");
    WULFHILDE_ASSERT(extent.width > 0 && extent.height > 0, "an image has at least one pixel");
    return drawing_->render_image(*device_, camera, extent);
}

} // namespace wulfhilde
