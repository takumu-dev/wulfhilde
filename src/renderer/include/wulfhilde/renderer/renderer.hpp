/// \file
/// The renderer, which draws with the GPU through Vulkan.

#pragma once

#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <expected>
#include <memory>

namespace wulfhilde {

namespace detail {

/// The Vulkan device a renderer draws with, and everything it owns on it.
class Device;

} // namespace detail

/// Draws with the GPU, through Vulkan 1.3 or newer.
///
/// It comes up only on a device that has every feature it needs: descriptor indexing,
/// `multiDrawIndirect` and `drawIndirectCount`, `shaderClipDistance`, `scalarBlockLayout`,
/// `samplerAnisotropy`, and what Vulkan 1.3 guarantees. It takes up Vulkan 1.4 where the device
/// has it, and never needs it. Like every module, it is used from one thread.
class Renderer {
  public:
    /// Creates a renderer that draws into images, with no window.
    ///
    /// It takes the first device that has every feature it needs, preferring a GPU of its own to
    /// one built into the processor, and either of those to any other.
    ///
    /// \param settings How the renderer is set up.
    /// \return The renderer, or why it couldn't come up, naming what each device lacks when none
    ///         has every feature it needs.
    [[nodiscard]] static std::expected<Renderer, DeviceError>
    create_headless(RendererSettings settings);

    /// Takes over another renderer's device, leaving that renderer empty, fit only to be destroyed
    /// or assigned to.
    ///
    /// \param other The renderer to take from.
    Renderer(Renderer&& other) noexcept;

    /// Takes over another renderer's device, releasing this one's first.
    ///
    /// \param other The renderer to take from.
    /// \return This renderer.
    Renderer& operator=(Renderer&& other) noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Waits for the device to finish its work, then releases it.
    ~Renderer();

  private:
    /// Makes a renderer of a device that has come up.
    ///
    /// \param device The device.
    explicit Renderer(std::unique_ptr<detail::Device> device) noexcept;

    /// The device the renderer draws with.
    std::unique_ptr<detail::Device> device_;
};

} // namespace wulfhilde
