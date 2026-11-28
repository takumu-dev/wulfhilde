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
#include "ambient_response_tests_spirv.h"
#include "brdf_tests_spirv.h"
#include "exposure_tests_spirv.h"
#include "luminance_tests_spirv.h"
#include "matrix_layout_tests_spirv.h"
#include "specular_anti_aliasing_tests_spirv.h"
#include "srgb_tests_spirv.h"

using AgxTable = wulfhilde::detail::TableConstants;
using AmbientTable = wulfhilde::detail::TableConstants;
using BrdfTable = wulfhilde::detail::TableConstants;
using ExposureTable = wulfhilde::detail::TableConstants;
using LuminanceTable = wulfhilde::detail::TableConstants;
using MatrixTable = wulfhilde::detail::TableConstants;
using SrgbTable = wulfhilde::detail::TableConstants;
using WideningTable = wulfhilde::detail::TableConstants;

struct MatrixRow {
    wulfhilde::Matrix4 matrix;
    wulfhilde::Vector4 vector;
};

struct BrdfRow {
    wulfhilde::Vector3 base_color{};
    float metallic = 0.0F;
    float roughness = 0.0F;
    wulfhilde::Vector3 normal{};
    wulfhilde::Vector3 toward_eye{};
    wulfhilde::Vector3 toward_light{};
};

struct AmbientRow {
    wulfhilde::Vector3 base_color{};
    float metallic = 0.0F;
    float roughness = 0.0F;
    float normal_dot_eye = 0.0F;
};

struct WideningRow {
    float roughness = 0.0F;
    wulfhilde::Vector3 normal_dx{};
    wulfhilde::Vector3 normal_dy{};
};

#include "agx_tests_layout.hpp"                    // IWYU pragma: keep
#include "ambient_response_tests_layout.hpp"       // IWYU pragma: keep
#include "brdf_tests_layout.hpp"                   // IWYU pragma: keep
#include "exposure_tests_layout.hpp"               // IWYU pragma: keep
#include "luminance_tests_layout.hpp"              // IWYU pragma: keep
#include "matrix_layout_tests_layout.hpp"          // IWYU pragma: keep
#include "specular_anti_aliasing_tests_layout.hpp" // IWYU pragma: keep
#include "srgb_tests_layout.hpp"                   // IWYU pragma: keep

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

TEST_CASE_METHOD(GpuTest, "a luminance meters at the EV100 of a light meter calibrated at 12.5",
                 "[renderer]") {
    const std::array luminances{0.125F, 1.0F, 128.0F, 0.0F};

    const auto ev100s = run_table<float>(exposure_tests_spirv, "ev100_of_luminances", luminances);

    using Catch::Matchers::WithinAbs;
    CHECK_THAT(ev100s.at(0), WithinAbs(0.0, 1e-5));
    CHECK_THAT(ev100s.at(1), WithinAbs(3.0, 1e-5));
    CHECK_THAT(ev100s.at(2), WithinAbs(10.0, 1e-5));
    // No light meters as the darkest exposure there is, log2 of 1e-10, not as minus infinity.
    CHECK_THAT(ev100s.at(3), WithinAbs(-33.219281, 1e-4));
}

TEST_CASE_METHOD(GpuTest,
                 "an exposure takes the luminance that saturates a sensor at its EV100 to 1",
                 "[renderer]") {
    const std::array ev100s{0.0F, 6.0F, -1.0F, 10.0F};

    const auto exposures = run_table<float>(exposure_tests_spirv, "exposure_of_ev100s", ev100s);

    // A sensor saturates at 1.2 times 2 to the EV100 nits.
    using Catch::Matchers::WithinRel;
    CHECK_THAT(exposures.at(0), WithinRel(0.8333333, 1e-5));
    CHECK_THAT(exposures.at(1), WithinRel(0.0130208, 1e-5));
    CHECK_THAT(exposures.at(2), WithinRel(1.6666667, 1e-5));
    CHECK_THAT(exposures.at(3), WithinRel(0.000813802, 1e-5));
}

namespace {

constexpr wulfhilde::Vector3 white{.x = 1.0F, .y = 1.0F, .z = 1.0F};
constexpr wulfhilde::Vector3 black{};
constexpr wulfhilde::Vector3 up{.z = 1.0F};

// Checks each channel of a colour against the one expected, within a tolerance.
void check_color(const wulfhilde::Vector3& actual, const wulfhilde::Vector3& expected,
                 double tolerance) {
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(actual.x, WithinAbs(static_cast<double>(expected.x), tolerance));
    CHECK_THAT(actual.y, WithinAbs(static_cast<double>(expected.y), tolerance));
    CHECK_THAT(actual.z, WithinAbs(static_cast<double>(expected.z), tolerance));
}

wulfhilde::Vector3 grey(float value) { return {.x = value, .y = value, .z = value}; }

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "the BRDF is glTF's: Lambertian diffuse under GGX specular with height-correlated "
                 "Smith visibility and Schlick's Fresnel, mixed by metalness",
                 "[renderer]") {
    const wulfhilde::Vector3 gold{.x = 1.0F, .y = 0.78F, .z = 0.34F};
    const wulfhilde::Vector3 from_west{.x = -1.0F, .z = 1.0F};
    const wulfhilde::Vector3 from_east{.x = 1.0F, .z = 1.0F};
    const std::array rows{
        // Fully rough white, lit and seen head on: the diffuse lobe takes all but the 4% that
        // Fresnel reflects, and GGX spreads that over the hemisphere.
        BrdfRow{
            .base_color = white,
            .roughness = 1.0F,
            .normal = up,
            .toward_eye = up,
            .toward_light = up,
        },
        // A metal at the peak of its highlight reflects its base colour, with no diffuse lobe.
        BrdfRow{
            .base_color = gold,
            .metallic = 1.0F,
            .roughness = 0.5F,
            .normal = up,
            .toward_eye = up,
            .toward_light = up,
        },
        // Mirrored about the normal at 45°, where Fresnel has begun to rise.
        BrdfRow{
            .base_color = grey(0.5F),
            .roughness = 0.5F,
            .normal = up,
            .toward_eye = from_west,
            .toward_light = from_east,
        },
        // Half metal is halfway between the dielectric's response and the metal's.
        BrdfRow{
            .base_color = grey(0.5F),
            .metallic = 0.5F,
            .roughness = 0.5F,
            .normal = up,
            .toward_eye = from_west,
            .toward_light = from_east,
        },
        // Away from the highlight's peak.
        BrdfRow{
            .base_color = grey(0.5F),
            .roughness = 0.3F,
            .normal = up,
            .toward_eye = up,
            .toward_light = from_east,
        },
        // A light behind the surface, and an eye behind it, see nothing.
        BrdfRow{
            .base_color = white,
            .roughness = 0.3F,
            .normal = up,
            .toward_eye = up,
            .toward_light = {.x = 1.0F, .z = -1.0F},
        },
        BrdfRow{
            .base_color = white,
            .roughness = 0.3F,
            .normal = up,
            .toward_eye = {.x = 1.0F, .z = -1.0F},
            .toward_light = up,
        },
    };

    const auto values = run_table<wulfhilde::Vector3>(brdf_tests_spirv, "brdf_of_rows", rows);

    // The values of glTF's formulas, worked out in double.
    check_color(values.at(0), grey(0.3087606F), 1e-5);
    check_color(values.at(1), {.x = 1.2732395F, .y = 0.9931268F, .z = 0.4329014F}, 1e-4);
    check_color(values.at(2), grey(0.2563893F), 1e-5);
    check_color(values.at(3), grey(0.7288745F), 1e-4);
    check_color(values.at(4), grey(0.1543357F), 1e-5);
    check_color(values.at(5), black, 1e-6);
    check_color(values.at(6), black, 1e-6);
}

TEST_CASE_METHOD(GpuTest,
                 "a material reflects its share of uniform ambient light: all of it when white, "
                 "metal or not, rough or not",
                 "[renderer]") {
    const std::array rows{
        AmbientRow{.base_color = white, .roughness = 1.0F, .normal_dot_eye = 1.0F},
        AmbientRow{.base_color = white, .metallic = 1.0F, .normal_dot_eye = 1.0F},
        AmbientRow{
            .base_color = white,
            .metallic = 1.0F,
            .roughness = 1.0F,
            .normal_dot_eye = 1.0F,
        },
        AmbientRow{
            .base_color = white,
            .metallic = 1.0F,
            .roughness = 0.5F,
            .normal_dot_eye = 0.3F,
        },
        // A smooth black dielectric reflects about 4% seen head on, and all of it at grazing.
        AmbientRow{.base_color = black, .normal_dot_eye = 1.0F},
        AmbientRow{.base_color = black, .normal_dot_eye = 0.17F},
        AmbientRow{.base_color = black, .normal_dot_eye = 0.0F},
        // A rough mid-grey dielectric reflects about its base colour.
        AmbientRow{.base_color = grey(0.18F), .roughness = 1.0F, .normal_dot_eye = 1.0F},
        // A mid-grey metal reflects its base colour when smooth, and less when rough.
        AmbientRow{.base_color = grey(0.5F), .metallic = 1.0F, .normal_dot_eye = 1.0F},
        AmbientRow{
            .base_color = grey(0.5F),
            .metallic = 1.0F,
            .roughness = 1.0F,
            .normal_dot_eye = 1.0F,
        },
        // A red dielectric keeps its colour under the white of its specular reflection.
        AmbientRow{
            .base_color = {.x = 0.8F, .y = 0.05F, .z = 0.05F},
            .roughness = 0.5F,
            .normal_dot_eye = 0.5F,
        },
    };

    const auto responses = run_table<wulfhilde::Vector3>(ambient_response_tests_spirv,
                                                         "ambient_response_of_rows", rows);

    for (std::size_t row = 0; row < 4; ++row) {
        CAPTURE(row);
        check_color(responses.at(row), white, 1e-5);
    }
    check_color(responses.at(4), grey(0.0456380F), 1e-5);
    check_color(responses.at(5), grey(0.3785347F), 1e-4);
    check_color(responses.at(6), white, 1e-5);
    check_color(responses.at(7), grey(0.1935075F), 1e-5);
    check_color(responses.at(8), grey(0.5029365F), 1e-5);
    check_color(responses.at(9), grey(0.3143679F), 1e-5);
    check_color(responses.at(10), {.x = 0.8102179F, .y = 0.0985352F, .z = 0.0985352F}, 1e-5);
}

TEST_CASE_METHOD(GpuTest,
                 "specular anti-aliasing widens roughness by how fast the shading normal changes "
                 "across the screen, up to a cap",
                 "[renderer]") {
    const std::array rows{
        // A normal that doesn't change leaves the roughness as it is.
        WideningRow{.roughness = 0.5F},
        WideningRow{.roughness = 0.5F, .normal_dx = {.x = 0.1F}},
        WideningRow{.roughness = 0.2F, .normal_dx = {.y = 0.05F}, .normal_dy = {.z = 0.05F}},
        // A normal that turns a radian a pixel adds the most roughness there is, even to a mirror.
        WideningRow{.roughness = 0.0F, .normal_dx = {.x = 1.0F}, .normal_dy = {.y = 1.0F}},
        WideningRow{.roughness = 0.0F, .normal_dx = {.x = 4.0F}, .normal_dy = {.y = 4.0F}},
        // Nothing is rougher than fully rough.
        WideningRow{.roughness = 1.0F, .normal_dx = {.x = 1.0F}},
    };

    const auto widened =
        run_table<float>(specular_anti_aliasing_tests_spirv, "widened_roughness_of_rows", rows);

    // The variance is 0.15 times the squared changes, and twice it adds to alpha squared, up to
    // 0.2: Filament's defaults.
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(widened.at(0), WithinAbs(0.5, 1e-6));
    CHECK_THAT(widened.at(1), WithinAbs(0.5058949, 1e-5));
    CHECK_THAT(widened.at(2), WithinAbs(0.2359611, 1e-5));
    CHECK_THAT(widened.at(3), WithinAbs(0.6687403, 1e-5));
    CHECK_THAT(widened.at(4), WithinAbs(0.6687403, 1e-5));
    CHECK_THAT(widened.at(5), WithinAbs(1.0, 1e-6));
}
