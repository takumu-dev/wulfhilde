/// \file
/// The renderer's tests' way into it: a device that runs shaders the tests supply. It is built
/// only with the tests, and never installed.

#pragma once

#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

/// The device a renderer draws with.
class Device;

/// The push constants of a table test's compute entry point, the twin of each such shader's own
/// table struct, which the test names after it: invocation `i` reads row `i` of the inputs and
/// writes row `i` of the results, for each of the rows.
struct TableConstants {
    /// The number of rows in the table.
    std::uint32_t row_count = 0;

    /// The device address of the inputs, one row after another.
    std::uint64_t inputs = 0;

    /// The device address of the results, one row after another.
    std::uint64_t results = 0;
};

/// What a test draws with `TestDevice::draw`.
struct TestDrawing {
    /// The SPIR-V module that holds both entry points.
    std::span<const std::uint32_t> spirv;

    /// The vertex entry point's name.
    std::string_view vertex_entry_point;

    /// The fragment entry point's name.
    std::string_view fragment_entry_point;

    /// The values of the module's 32-bit specialisation constants, the one with ID `i` taking
    /// value `i`.
    std::span<const std::uint32_t> specialization_constants;

    /// The push constants both entry points see.
    std::span<const std::byte> push_constants;

    /// The number of vertices to draw, a multiple of three.
    std::size_t vertex_count = 0;

    /// The image's size, neither side zero.
    Extent extent{};
};

/// A device made as the renderer makes its own, which runs the tests' shaders in the renderer's
/// pipeline layout and bindless set.
class TestDevice {
  public:
    /// The number of invocations in each group of a table test's compute entry point.
    static constexpr std::size_t table_group_size = 64;

    /// Comes up on a device as a renderer would.
    ///
    /// \param settings How the device is set up, as a renderer's would be.
    /// \param newest_minor_version The newest Vulkan 1.x to take the device to have, which may
    ///        be older than its own to stand in for an older device.
    /// \return The device, or why it couldn't come up.
    [[nodiscard]] static std::expected<TestDevice, DeviceError>
    create(RendererSettings settings, std::uint32_t newest_minor_version = 4);

    /// Takes over another test device.
    ///
    /// \param other The device to take over.
    TestDevice(TestDevice&& other) noexcept;

    /// Takes over another test device, releasing this one's first.
    ///
    /// \param other The device to take over.
    /// \return This device.
    TestDevice& operator=(TestDevice&& other) noexcept;

    TestDevice(const TestDevice&) = delete;
    TestDevice& operator=(const TestDevice&) = delete;

    /// Waits for the device, then releases it.
    ~TestDevice();

    /// Runs a compute entry point over a table, in groups of `table_group_size`.
    ///
    /// It passes the inputs and a buffer for the results through push constants that are
    /// `TableConstants`, and returns the results once every invocation has finished.
    ///
    /// \param spirv The SPIR-V module that holds the entry point.
    /// \param entry_point The entry point's name.
    /// \param inputs The table's rows, one after another.
    /// \param row_count The number of rows in the table, more than zero.
    /// \param result_size The size of the results in bytes, more than zero.
    /// \return The results, or what failed on the device.
    [[nodiscard]] std::expected<std::vector<std::byte>, DeviceError>
    run_table(std::span<const std::uint32_t> spirv, std::string_view entry_point,
              std::span<const std::byte> inputs, std::size_t row_count, std::size_t result_size);

    /// Uploads an image as an sRGB texture with one mip level, and adds it to the bindless set.
    ///
    /// \param image The texture's pixels.
    /// \return The index of the texture in the bindless set's sampled images, which a
    ///         `DescriptorHandle<Texture2D>` holds first, or what failed on the device.
    [[nodiscard]] std::expected<std::uint32_t, DeviceError> add_texture(const Image& image);

    /// Adds a sampler that takes the nearest texel and clamps to the edges.
    ///
    /// \return The index of the sampler in the bindless set's samplers, which a
    ///         `DescriptorHandle<SamplerState>` holds first, or what failed on the device.
    [[nodiscard]] std::expected<std::uint32_t, DeviceError> add_nearest_sampler();

    /// Draws a list of triangles with no vertex buffer into an 8-bit sRGB image cleared to
    /// transparent black, and reads the image back.
    ///
    /// \param drawing What to draw, and how large an image to draw it into.
    /// \return The image, or what failed on the device.
    [[nodiscard]] std::expected<Image, DeviceError> draw(const TestDrawing& drawing);

    /// Breaks one of Vulkan's valid-usage rules, in a way that every driver survives: it makes a
    /// sampler whose anisotropy is far past any device's limit, then destroys it. Validation, when
    /// it is on, reports the misuse as an error.
    void break_valid_usage();

  private:
    /// The textures and samplers added to the bindless set.
    struct BindlessContents;

    /// Makes a test device of a device that has come up.
    ///
    /// \param device The device.
    explicit TestDevice(std::unique_ptr<Device> device);

    /// The device, which outlives the textures and samplers made on it.
    std::unique_ptr<Device> device_;

    /// The textures and samplers added to the bindless set, kept for as long as it may be used.
    std::unique_ptr<BindlessContents> contents_;
};

} // namespace wulfhilde::detail
