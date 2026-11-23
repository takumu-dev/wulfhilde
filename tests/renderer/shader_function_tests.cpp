#include "gpu_test.hpp"
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>
#include <wulfhilde/renderer/testing/test_device.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace {

#include "agx_tests_spirv.h"
#include "luminance_tests_spirv.h"
#include "matrix_layout_tests_spirv.h"
#include "srgb_tests_spirv.h"

using AgxTable = wulfhilde::detail::TableConstants;
using LuminanceTable = wulfhilde::detail::TableConstants;
using MatrixTable = wulfhilde::detail::TableConstants;
using SrgbTable = wulfhilde::detail::TableConstants;

struct MatrixRow {
    wulfhilde::Matrix4 matrix;
    wulfhilde::Vector4 vector;
};

#include "agx_tests_layout.hpp"           // IWYU pragma: keep
#include "luminance_tests_layout.hpp"     // IWYU pragma: keep
#include "matrix_layout_tests_layout.hpp" // IWYU pragma: keep
#include "srgb_tests_layout.hpp"          // IWYU pragma: keep

// Runs a table test's entry point over rows of inputs, on a device taken to have Vulkan 1.4 or
// the given older version, and reads back one result for each row.
template <typename Result, typename Input, std::size_t Count>
std::vector<Result> run_table(std::span<const std::uint32_t> spirv, const char* entry_point,
                              const std::array<Input, Count>& rows,
                              std::uint32_t vulkan_minor_version = 4) {
    const std::span inputs(rows);
    auto device = wulfhilde::detail::TestDevice::create(gpu_test_settings, vulkan_minor_version);
    require_value(device);
    const auto bytes = device->run_table(spirv, entry_point, std::as_bytes(inputs), inputs.size(),
                                         inputs.size() * sizeof(Result));
    require_value(bytes);
    std::vector<Result> results(inputs.size());
    std::memcpy(results.data(), bytes->data(), bytes->size());
    return results;
}

} // namespace

TEST_CASE_METHOD(GpuTest, "the renderer's luminance weights green most and blue least",
                 "[renderer]") {
    // Under Vulkan 1.3, pipelines take their SPIR-V through shader modules.
    const auto vulkan_minor_version = GENERATE(4U, 3U);
    CAPTURE(vulkan_minor_version);
    const std::array colors{
        wulfhilde::Vector3{.x = 1.0F, .y = 1.0F, .z = 1.0F},
        wulfhilde::Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F},
        wulfhilde::Vector3{.x = 1.0F},
        wulfhilde::Vector3{.y = 1.0F},
        wulfhilde::Vector3{.z = 1.0F},
        wulfhilde::Vector3{.x = 0.5F, .y = 0.5F, .z = 0.5F},
    };

    const auto luminances = run_table<float>(luminance_tests_spirv, "luminance_of_colors", colors,
                                             vulkan_minor_version);

    using Catch::Matchers::WithinAbs;
    CHECK_THAT(luminances.at(0), WithinAbs(1.0, 1e-6));
    CHECK_THAT(luminances.at(1), WithinAbs(0.0, 1e-6));
    CHECK_THAT(luminances.at(2), WithinAbs(0.2126, 1e-6));
    CHECK_THAT(luminances.at(3), WithinAbs(0.7152, 1e-6));
    CHECK_THAT(luminances.at(4), WithinAbs(0.0722, 1e-6));
    CHECK_THAT(luminances.at(5), WithinAbs(0.5, 1e-6));
}

TEST_CASE_METHOD(GpuTest,
                 "the renderer's sRGB encoding is linear near black and a power curve above",
                 "[renderer]") {
    const std::array colors{
        wulfhilde::Vector3{.x = 0.0F, .y = 0.001F, .z = 0.0031308F},
        wulfhilde::Vector3{.x = 0.18F, .y = 0.5F, .z = 1.0F},
    };

    const auto encoded =
        run_table<wulfhilde::Vector3>(srgb_tests_spirv, "srgb_encoded_colors", colors);

    using Catch::Matchers::WithinAbs;
    CHECK_THAT(encoded.at(0).x, WithinAbs(0.0, 1e-6));
    CHECK_THAT(encoded.at(0).y, WithinAbs(0.01292, 1e-6));
    CHECK_THAT(encoded.at(0).z, WithinAbs(0.0404482, 1e-5));
    CHECK_THAT(encoded.at(1).x, WithinAbs(0.4613561, 1e-5));
    CHECK_THAT(encoded.at(1).y, WithinAbs(0.7353570, 1e-5));
    CHECK_THAT(encoded.at(1).z, WithinAbs(1.0, 1e-5));
}

TEST_CASE_METHOD(GpuTest, "a matrix from C++ times a vector gives the same in the shaders",
                 "[renderer]") {
    // Every entry differs, so a matrix read row for column gives another product.
    const wulfhilde::Matrix4 matrix{
        .columns =
            {
                {
                    {.x = 1.0F, .y = 2.0F, .z = 3.0F, .w = 4.0F},
                    {.x = 5.0F, .y = 6.0F, .z = 7.0F, .w = 8.0F},
                    {.x = 9.0F, .y = 10.0F, .z = 11.0F, .w = 12.0F},
                    {.x = 13.0F, .y = 14.0F, .z = 15.0F, .w = 16.0F},
                },
            },
    };
    const std::array rows{
        MatrixRow{.matrix = matrix, .vector = {.x = 1.0F}},
        MatrixRow{.matrix = matrix, .vector = {.x = 1.0F, .y = -1.0F, .z = 0.5F, .w = 2.0F}},
    };

    const auto products =
        run_table<wulfhilde::Vector4>(matrix_layout_tests_spirv, "matrix_times_vector", rows);

    CHECK(products.at(0) == wulfhilde::Vector4{.x = 1.0F, .y = 2.0F, .z = 3.0F, .w = 4.0F});
    CHECK(products.at(1) == matrix * rows.at(1).vector);
}

TEST_CASE_METHOD(GpuTest,
                 "AgX takes the scene's light into the display's, rolling bright light off",
                 "[renderer]") {
    const std::array colors{
        wulfhilde::Vector3{.x = 0.0F},
        wulfhilde::Vector3{.x = 0.18F, .y = 0.18F, .z = 0.18F},
        wulfhilde::Vector3{.x = 1.0F, .y = 1.0F, .z = 1.0F},
        wulfhilde::Vector3{.x = 1000.0F, .y = 1000.0F, .z = 1000.0F},
        wulfhilde::Vector3{.x = 4.0F, .y = 0.5F, .z = 0.1F},
    };

    const auto displayed = run_table<wulfhilde::Vector3>(agx_tests_spirv, "agx_of_colors", colors);

    // The values of AgX's published matrices, stops and curve, decoded with a power of 2.2.
    using Catch::Matchers::WithinAbs;
    const auto check = [](const wulfhilde::Vector3& actual, const wulfhilde::Vector3& expected) {
        CHECK_THAT(actual.x, WithinAbs(static_cast<double>(expected.x), 2e-4));
        CHECK_THAT(actual.y, WithinAbs(static_cast<double>(expected.y), 2e-4));
        CHECK_THAT(actual.z, WithinAbs(static_cast<double>(expected.z), 2e-4));
    };
    check(displayed.at(0), {});
    check(displayed.at(1), {.x = 0.214467F, .y = 0.214533F, .z = 0.214537F});
    check(displayed.at(2), {.x = 0.589977F, .y = 0.590207F, .z = 0.590221F});
    check(displayed.at(3), {.x = 0.996502F, .y = 0.996978F, .z = 0.997007F});
    // A bright orange keeps its hue, red over green over blue, short of white.
    check(displayed.at(4), {.x = 0.949165F, .y = 0.476594F, .z = 0.275492F});
}
