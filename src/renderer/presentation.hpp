// How a renderer shows its frames in a window: a swapchain on the window's surface, made anew
// whenever the window's size changes.
#pragma once

#include "device.hpp"
#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

namespace wulfhilde::detail {

/// The format of a window's images, and whether the AgX pass encodes its colours in sRGB itself,
/// for a format that stores them as they are written.
struct WindowFormat {
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool encodes_srgb = false;
};

/// A swapchain's image that a frame is drawn into, and the semaphores around drawing it.
struct Frame {
    std::uint32_t index = 0;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    Extent extent;
    /// Signalled once the image may be written.
    VkSemaphore acquired = VK_NULL_HANDLE;
    /// Signalled by the frame's commands once the image may be presented.
    VkSemaphore rendered = VK_NULL_HANDLE;
};

/// Shows frames in a window through a swapchain on the device's surface, one frame at a time.
class Presentation {
    struct Swapchain;

    // Lets `create` alone call the public constructor that std::make_unique needs.
    struct Key {
        explicit Key() = default;
    };

  public:
    /// Chooses the format the window's images take: 8-bit sRGB, as the tone-mapping pass writes
    /// images, or else 8-bit colours shown in sRGB's colour space, which it encodes itself.
    ///
    /// \param device A device made for the window.
    /// \param window The window, which must outlive this.
    [[nodiscard]] static std::expected<std::unique_ptr<Presentation>, DeviceError>
    create(const Device& device, const Window& window);

    /// Takes the format that `create` chose.
    Presentation(Key /*key*/, const Window& window, WindowFormat format) noexcept;

    Presentation(const Presentation&) = delete;
    Presentation(Presentation&&) = delete;
    Presentation& operator=(const Presentation&) = delete;
    Presentation& operator=(Presentation&&) = delete;
    ~Presentation();

    /// The format of the window's images.
    [[nodiscard]] WindowFormat format() const noexcept { return format_; }

    /// Acquires the image to draw the next frame into, first making the swapchain anew if the
    /// window's size has changed or the swapchain no longer fits its surface.
    ///
    /// \return The frame, or none when the window has no area to show one, or what failed.
    [[nodiscard]] std::expected<std::optional<Frame>, DeviceError> begin_frame(Device& device);

    /// Presents a frame's image once its commands signal that it is rendered.
    [[nodiscard]] std::expected<void, DeviceError> end_frame(const Device& device,
                                                             const Frame& frame);

  private:
    // Makes the swapchain anew at an extent, from the one before it, once the device is idle.
    [[nodiscard]] std::expected<void, DeviceError> remake_swapchain(const Device& device,
                                                                    Extent extent);

    const Window& window_;
    WindowFormat format_;
    std::unique_ptr<Swapchain> swapchain_;
    // Whether the surface has said the swapchain no longer fits it.
    bool stale_ = false;
};

} // namespace wulfhilde::detail
