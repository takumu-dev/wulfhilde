/// \file
/// The renderer, which draws with the GPU through Vulkan.

#pragma once

#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <memory>

namespace wulfhilde {

namespace detail {

/// The Vulkan device a renderer draws with, and everything it owns on it.
class Device;

/// What a renderer draws with on its device: its pipelines, the map's geometry, and its targets.
class Drawing;

/// How a renderer shows its frames in a window: the window's surface and swapchain.
class Presentation;

} // namespace detail

/// Draws with the GPU, through Vulkan 1.3 or newer.
///
/// It comes up only on a device that has every feature it needs: descriptor indexing,
/// `multiDrawIndirect` and `drawIndirectCount`, `shaderClipDistance`, `scalarBlockLayout`,
/// `samplerAnisotropy`, a 32-bit float depth format with stencil, and what Vulkan 1.3 guarantees.
/// It takes up Vulkan 1.4 where the device has it, and never needs it. Like every module, it is
/// used from one thread. It draws frames into a window, or images into memory, or both.
///
/// It draws a map in linear light into a multisampled target of 16-bit floats, with 32-bit float
/// depth, reversed so that it is 1 at the near plane, 5 cm from the eye, and 0 infinitely far
/// away. AgX then tone-maps each sample, averages a pixel's samples, and writes the image in sRGB.
class Renderer {
  public:
    /// Creates a renderer that draws frames into a window, and images too.
    ///
    /// It takes the first device that has every feature it needs and can show frames in the
    /// window, preferring a GPU of its own to one built into the processor, and either of those to
    /// any other.
    ///
    /// \param window The window, which must outlive the renderer.
    /// \param settings How the renderer is set up.
    /// \return The renderer, or why it couldn't come up, naming what each device lacks when none
    ///         has every feature it needs, or the sample counts the device has when it lacks the
    ///         one asked for.
    [[nodiscard]] static std::expected<Renderer, DeviceError> create(Window& window,
                                                                     RendererSettings settings);

    /// Creates a renderer that draws into images, with no window.
    ///
    /// It takes the first device that has every feature it needs, preferring a GPU of its own to
    /// one built into the processor, and either of those to any other.
    ///
    /// \param settings How the renderer is set up.
    /// \return The renderer, or why it couldn't come up, naming what each device lacks when none
    ///         has every feature it needs, or the sample counts the device has when it lacks the
    ///         one asked for.
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

    /// Uploads a map to the device, in place of any map set before, to draw from then on.
    ///
    /// Each sector is triangulated into a range of vertices of its own: its walls, and its floor
    /// and ceiling, concave or not. Every surface is drawn opaque, from the front only, in its
    /// material's base colour under its sector's ambient light.
    ///
    /// \param map The map. The renderer keeps a reference to it, so it must outlive the renderer,
    ///            or the next call of `set_map`.
    /// \param assets The map's assets, loaded for this map. The renderer copies what it needs.
    /// \return Nothing, or what failed on the device, which leaves no map set.
    [[nodiscard]] std::expected<void, DeviceError> set_map(const Map& map, const MapAssets& assets);

    /// Draws a frame of the map that is set, as a camera sees it, and shows it in the window.
    ///
    /// The frame fills the window's drawable area, at its pixels. When the window's size has
    /// changed, the renderer first makes its swapchain and targets anew at the new size. A window
    /// with no area, such as one minimised, shows nothing, and the frame is skipped. A frame is
    /// shown at the display's next refresh, which paces a program that draws in a loop.
    ///
    /// \param camera Where the frame is seen from, in a sector of the map that is set.
    /// \return Nothing, or what failed on the device.
    [[nodiscard]] std::expected<void, DeviceError> draw(const Camera& camera);

    /// Draws an image of the map that is set, as a camera sees it.
    ///
    /// \param camera Where the image is seen from, in a sector of the map that is set.
    /// \param extent The image's size, neither side of it zero.
    /// \return The image, or what failed on the device.
    [[nodiscard]] std::expected<Image, DeviceError> render_image(const Camera& camera,
                                                                 Extent extent);

  private:
    /// Makes a renderer on a device, for a window or for none.
    ///
    /// \param window The window to draw frames into, or none.
    /// \param settings How the renderer is set up.
    /// \return The renderer, or why it couldn't come up.
    [[nodiscard]] static std::expected<Renderer, DeviceError> create_for(Window* window,
                                                                         RendererSettings settings);

    /// Makes a renderer of a device that has come up, with what it draws with there.
    ///
    /// \param device The device.
    /// \param drawing What the renderer draws with on the device.
    /// \param presentation How it shows frames in its window, or none for a renderer without one.
    Renderer(std::unique_ptr<detail::Device> device, std::unique_ptr<detail::Drawing> drawing,
             std::unique_ptr<detail::Presentation> presentation) noexcept;

    /// The device the renderer draws with, which outlives what is made on it.
    std::unique_ptr<detail::Device> device_;

    /// What the renderer draws with on its device.
    std::unique_ptr<detail::Drawing> drawing_;

    /// How the renderer shows frames in its window, or none for a renderer without one.
    std::unique_ptr<detail::Presentation> presentation_;

    /// The map that is set, or none.
    const Map* map_ = nullptr;
};

} // namespace wulfhilde
