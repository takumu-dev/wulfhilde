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

#include "luminance_tests_spirv.h"
#include "matrix_layout_tests_spirv.h"

using LuminanceTable = wulfhilde::detail::TableConstants;
using MatrixTable = wulfhilde::detail::TableConstants;

struct MatrixRow {
    wulfhilde::Matrix4 matrix;
    wulfhilde::Vector4 vector;
};

#include "luminance_tests_layout.hpp"     // IWYU pragma: keep
#include "matrix_layout_tests_layout.hpp" // IWYU pragma: keep

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
