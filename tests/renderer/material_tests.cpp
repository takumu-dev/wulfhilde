// How the renderer shades materials under ambient light: how much of it each reflects, by its base
// colour, metalness and roughness, how its normal texture tilts it and its occlusion darkens it,
// and how its emission glows, at its own nits or at a surface's.
#include <wulfhilde/assets/material.hpp>

#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Color;
using wulfhilde::Material;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

using Texel = std::array<std::uint8_t, 4>;
using Pixel = std::array<std::uint8_t, 4>;

constexpr Color white{.r = 1.0F, .g = 1.0F, .b = 1.0F};
constexpr Color black{};

// The ambient light the tests' pinned exposure shows a mid-grey surface under as a mid-grey.
constexpr float ambient_lux = 250.0F;

// An image of one texel, read in a colour space, which a material repeats everywhere.
wulfhilde::TextureImage flat_image(std::string path, Texel texel,
                                   wulfhilde::ColorSpace color_space) {
    return {
        .path = std::move(path),
        .color_space = color_space,
        .levels = {{.width = 1, .height = 1, .texels = {texel.begin(), texel.end()}}},
    };
}

// An image of two texels side by side, read linearly: the left one along -u, the right along +u.
wulfhilde::TextureImage halves(std::string path, Texel left, Texel right) {
    wulfhilde::MipLevel full{.width = 2, .height = 1};
    full.texels.insert(full.texels.end(), left.begin(), left.end());
    full.texels.insert(full.texels.end(), right.begin(), right.end());
    Texel average{};
    for (std::size_t channel = 0; channel < average.size(); ++channel) {
        average.at(channel) = static_cast<std::uint8_t>((left.at(channel) + right.at(channel)) / 2);
    }
    return {
        .path = std::move(path),
        .color_space = wulfhilde::ColorSpace::linear,
        .levels = {std::move(full),
                   {.width = 1, .height = 1, .texels = {average.begin(), average.end()}}},
    };
}

// A rough dielectric of a base colour.
Material plain(Color color) {
    return {
        .color = {.factor = color}, .roughness = {.factor = 1.0F}, .metallic = {.factor = 0.0F}};
}

// The box: a square sector 4 m on a side from (0, 0) to (4, 4), 3 m high, under white ambient light
// of an illuminance. Its floor's material is "floor", its east wall's "east", and every other
// surface's "rest".
wulfhilde::MapDescription box_map(float lux = ambient_lux) {
    const auto wall = [](float x, float y, const char* material) {
        return wulfhilde::WallDescription{.start = {.x = x, .y = y},
                                          .surface = {.material = material}};
    };
    return {
        .ambient = wulfhilde::AmbientLight{.color = white, .lux = lux},
        .sectors =
            {
                {
                    .id = "box",
                    .floor = {.height = 0.0F, .material = "floor"},
                    .ceiling = {.height = 3.0F, .material = "rest"},
                    .loops = {{
                        wall(0.0F, 0.0F, "rest"),
                        wall(4.0F, 0.0F, "east"),
                        wall(4.0F, 4.0F, "rest"),
                        wall(0.0F, 4.0F, "rest"),
                    }},
                },
            },
        .start = {.sector = "box", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The assets of the box, every material a mid-grey rough dielectric but those given.
wulfhilde::MapAssets assets_of(const wulfhilde::Map& map,
                               const std::map<std::string, Material>& materials,
                               std::vector<wulfhilde::TextureImage> images = {}) {
    wulfhilde::MapAssets assets{.images = std::move(images)};
    for (const auto& path : map.materials()) {
        const auto given = materials.find(path);
        assets.materials.push_back(
            given != materials.end() ? given->second : plain({.r = 0.18F, .g = 0.18F, .b = 0.18F}));
    }
    return assets;
}

constexpr wulfhilde::Extent image_extent{.width = 128, .height = 128};

// A camera with a field of view of 90°, in the box.
wulfhilde::Camera camera_at(const wulfhilde::Map& map, Vector3 eye, Angle yaw, Angle pitch) {
    return {
        .location = {.sector = map.start().location.sector, .point = eye},
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, Angle::degrees(0.0F)),
        .vertical_field_of_view = Angle::degrees(90.0F),
    };
}

wulfhilde::Image render(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets,
                        const wulfhilde::Camera& camera) {
    auto renderer = wulfhilde::Renderer::create_headless(gpu_test_settings);
    require_value(renderer);
    require_value(renderer->set_map(map, assets));
    auto image = renderer->render_image(camera, image_extent);
    require_value(image);
    return *std::move(image);
}

// What a camera sees of the box, in an image.
class View {
  public:
    View(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets, Vector3 eye, Angle yaw,
         Angle pitch)
        : camera_(camera_at(map, eye, yaw, pitch)), image_(render(map, assets, camera_)) {}

    // The pixel a point of the box lies in, projected as the renderer projects it.
    [[nodiscard]] Pixel at(Vector3 point) const {
        const auto relative = wulfhilde::rotate(wulfhilde::inverse(camera_.orientation),
                                                point - camera_.location.point);
        const auto width = static_cast<float>(image_extent.width);
        const auto height = static_cast<float>(image_extent.height);
        const auto vertical = 1.0F / std::tan(camera_.vertical_field_of_view.in_radians() / 2.0F);
        const auto horizontal = vertical * height / width;
        // Forward, left and up are x, y and z in the camera's own axes.
        const auto x = (1.0F - (horizontal * relative.y / relative.x)) * width / 2.0F;
        const auto y = (1.0F - (vertical * relative.z / relative.x)) * height / 2.0F;
        REQUIRE(relative.x > 0.0F);
        REQUIRE((x >= 0.0F && x < width && y >= 0.0F && y < height));
        const auto start =
            ((static_cast<std::size_t>(y) * image_extent.width) + static_cast<std::size_t>(x)) *
            wulfhilde::Image::bytes_per_pixel;
        Pixel pixel{};
        std::ranges::copy(image_.pixels().subspan(start, pixel.size()), pixel.begin());
        return pixel;
    }

  private:
    wulfhilde::Camera camera_;
    wulfhilde::Image image_;
};

// The middle of the floor, seen straight down from 1.5 m above it.
Pixel floor_from_above(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets) {
    const View seen(map, assets, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F),
                    Angle::degrees(-90.0F));
    return seen.at({.x = 2.0F, .y = 2.0F, .z = 0.0F});
}

// A point of the floor seen from its west side, low, looking east along it, so that the floor
// faces the eye at a slant: the way to the eye leans toward -X.
Pixel floor_from_the_west(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets) {
    const View seen(map, assets, {.x = 0.5F, .y = 2.0F, .z = 1.0F}, Angle::degrees(0.0F),
                    Angle::degrees(-30.0F));
    return seen.at({.x = 2.0F, .y = 2.0F, .z = 0.0F});
}

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

// Whether two pixels match in every channel, give or take a step of rounding.
bool matches(const Pixel& a, const Pixel& b) {
    return std::ranges::all_of(std::array{0, 1, 2}, [&](std::size_t channel) {
        return std::abs(int{a.at(channel)} - int{b.at(channel)}) <= 1;
    });
}

// The luminance in nits that a white surface reflects under ambient light of an illuminance, as a
// Lambertian surface does.
float reflected_nits(float lux) { return lux / std::numbers::pi_v<float>; }

// A black material that glows white at a luminance, and reflects only its 4% of light.
Material glowing(float nits, Color color = white) {
    auto material = plain(black);
    material.emission = wulfhilde::Emission{.nits = nits, .color = color};
    return material;
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a white surface reflects all the ambient light that reaches it, metal or not, "
                 "rough or not, as bright as a glow at the light's luminance",
                 "[renderer]") {
    auto dark_description = box_map();
    dark_description.ambient.reset();
    const auto dark = make_map(std::move(dark_description));
    const auto glow =
        floor_from_above(dark, assets_of(dark, {{"floor", glowing(reflected_nits(ambient_lux))}}));
    const auto lit = make_map(box_map());

    for (const auto& [metallic, roughness] : {
             std::pair{0.0F, 1.0F},
             std::pair{0.0F, 0.3F},
             std::pair{1.0F, 1.0F},
             std::pair{1.0F, 0.0F},
         }) {
        CAPTURE(metallic, roughness);
        const auto reflected =
            floor_from_above(lit, assets_of(lit, {{"floor",
                                                   {
                                                       .color = {.factor = white},
                                                       .roughness = {.factor = roughness},
                                                       .metallic = {.factor = metallic},
                                                   }}}));

        CAPTURE(describe(reflected), describe(glow));
        CHECK(matches(reflected, glow));
    }
}

TEST_CASE_METHOD(GpuTest,
                 "a rough metal reflects less of the ambient light than a smooth one, and a "
                 "dielectric of the same colour more, by its metalness and roughness",
                 "[renderer]") {
    const auto map = make_map(box_map());
    const auto grey_floor = [&map](float metallic, float roughness) {
        return floor_from_above(
            map, assets_of(map, {{"floor",
                                  {
                                      .color = {.factor = {.r = 0.5F, .g = 0.5F, .b = 0.5F}},
                                      .roughness = {.factor = roughness},
                                      .metallic = {.factor = metallic},
                                  }}}));
    };

    const auto rough_metal = grey_floor(1.0F, 1.0F);
    const auto smooth_metal = grey_floor(1.0F, 0.0F);
    const auto dielectric = grey_floor(0.0F, 1.0F);

    CAPTURE(describe(rough_metal), describe(smooth_metal), describe(dielectric));
    CHECK(rough_metal.at(1) + 16 < smooth_metal.at(1));
    CHECK(smooth_metal.at(1) <= dielectric.at(1));
}

TEST_CASE_METHOD(GpuTest,
                 "metalness and roughness are their factors times the channels of their textures "
                 "that they name",
                 "[renderer]") {
    // One texture holds the roughness in green and the metalness in blue, as glTF packs them: the
    // left half rough and the right half smooth, both metal.
    const auto map = make_map(box_map());
    const auto packed = halves("packed", {0, 255, 255, 255}, {0, 0, 255, 255});
    const auto grey = Color{.r = 0.5F, .g = 0.5F, .b = 0.5F};
    const Material textured{
        .color = {.factor = grey},
        .roughness = {.factor = 1.0F, .texture = 0, .channel = wulfhilde::Channel::g},
        .metallic = {.factor = 1.0F, .texture = 0, .channel = wulfhilde::Channel::b},
        .size = Vector2{.x = 4.0F, .y = 4.0F},
    };
    const View seen(map, assets_of(map, {{"floor", textured}}, {packed}),
                    {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F),
                    Angle::degrees(-90.0F));
    const auto rough_half = seen.at({.x = 1.0F, .y = 2.0F, .z = 0.0F});
    const auto smooth_half = seen.at({.x = 3.0F, .y = 2.0F, .z = 0.0F});

    const auto rough_metal =
        floor_from_above(map, assets_of(map, {{"floor",
                                               {
                                                   .color = {.factor = grey},
                                                   .roughness = {.factor = 1.0F},
                                                   .metallic = {.factor = 1.0F},
                                               }}}));
    // Without metalness, the factor of 0 holds whatever the texture says.
    auto dielectric = textured;
    dielectric.metallic.factor = 0.0F;
    const auto dielectric_rough_half =
        View(map, assets_of(map, {{"floor", dielectric}}, {packed}),
             {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F), Angle::degrees(-90.0F))
            .at({.x = 1.0F, .y = 2.0F, .z = 0.0F});

    CAPTURE(describe(rough_half), describe(smooth_half), describe(rough_metal),
            describe(dielectric_rough_half));
    CHECK(matches(rough_half, rough_metal));
    CHECK(rough_half.at(1) + 16 < smooth_half.at(1));
    CHECK(rough_half.at(1) + 16 < dielectric_rough_half.at(1));
}

TEST_CASE_METHOD(GpuTest,
                 "ambient occlusion darkens the ambient light a material reflects, by its strength",
                 "[renderer]") {
    const auto lit = make_map(box_map());
    const auto half_lit = make_map(box_map(ambient_lux / 2.0F));
    const auto occluded = [](float factor, float strength) {
        auto material = plain(white);
        material.occlusion = wulfhilde::OcclusionProperty{
            .value = {.factor = factor},
            .strength = strength,
        };
        return material;
    };

    const auto unoccluded = floor_from_above(lit, assets_of(lit, {{"floor", plain(white)}}));
    const auto half_light =
        floor_from_above(half_lit, assets_of(half_lit, {{"floor", plain(white)}}));
    const auto half_occluded =
        floor_from_above(lit, assets_of(lit, {{"floor", occluded(0.5F, 1.0F)}}));
    const auto at_no_strength =
        floor_from_above(lit, assets_of(lit, {{"floor", occluded(0.5F, 0.0F)}}));
    // At half strength, an occlusion of 0 lets half the light through.
    const auto at_half_strength =
        floor_from_above(lit, assets_of(lit, {{"floor", occluded(0.0F, 0.5F)}}));
    // A grey texture's channel gives the occlusion, times the factor.
    auto textured = plain(white);
    textured.occlusion = wulfhilde::OcclusionProperty{
        .value = {.factor = 1.0F, .texture = 0, .channel = wulfhilde::Channel::g},
    };
    textured.size = Vector2{.x = 1.0F, .y = 1.0F};
    const auto from_texture = floor_from_above(
        lit, assets_of(lit, {{"floor", textured}},
                       {flat_image("half", {0, 128, 0, 255}, wulfhilde::ColorSpace::linear)}));

    CAPTURE(describe(unoccluded), describe(half_light), describe(half_occluded),
            describe(at_no_strength), describe(at_half_strength), describe(from_texture));
    CHECK(matches(half_occluded, half_light));
    CHECK(matches(at_no_strength, unoccluded));
    CHECK(matches(at_half_strength, half_light));
    CHECK(matches(from_texture, half_light));
}

TEST_CASE_METHOD(GpuTest,
                 "a material's emission glows at its nits in its colour, on top of what it "
                 "reflects, and lights nothing else",
                 "[renderer]") {
    // Under no ambient light, a black floor that glows at the luminance a white surface reflects
    // under the tests' ambient light shows as that white surface does under it.
    auto dark_description = box_map();
    dark_description.ambient.reset();
    const auto dark = make_map(std::move(dark_description));
    const auto lit = make_map(box_map());
    const auto glow = floor_from_above(
        dark, assets_of(dark, {{"floor", glowing(reflected_nits(ambient_lux), white)}}));
    const auto white_floor = floor_from_above(lit, assets_of(lit, {{"floor", plain(white)}}));
    // A red glow is red.
    const auto red_glow = floor_from_above(
        dark, assets_of(dark, {{"floor", glowing(100.0F, {.r = 1.0F, .g = 0.05F, .b = 0.05F})}}));

    // The floor beside a glowing east wall is as it is beside a plain one.
    const auto beside = [&lit](const Material& east) {
        const View seen(lit, assets_of(lit, {{"east", east}}), {.x = 1.0F, .y = 2.0F, .z = 1.5F},
                        Angle::degrees(0.0F), Angle::degrees(-45.0F));
        return std::pair{seen.at({.x = 3.8F, .y = 2.0F, .z = 0.0F}),
                         seen.at({.x = 4.0F, .y = 2.0F, .z = 1.0F})};
    };
    const auto [floor_beside_glow, glowing_wall] = beside(glowing(2'000.0F));
    const auto [floor_beside_plain, plain_wall] = beside(plain(black));

    CAPTURE(describe(glow), describe(white_floor), describe(red_glow), describe(floor_beside_glow),
            describe(floor_beside_plain), describe(glowing_wall), describe(plain_wall));
    CHECK(matches(glow, white_floor));
    CHECK(red_glow.at(0) > red_glow.at(1) + 64);
    CHECK(glowing_wall.at(0) > plain_wall.at(0) + 64);
    CHECK(matches(floor_beside_glow, floor_beside_plain));
}

TEST_CASE_METHOD(GpuTest,
                 "a surface's nits replace its material's emission level, keeping its colour and "
                 "its texture",
                 "[renderer]") {
    auto description = box_map();
    description.ambient.reset();
    auto dimmed_description = description;
    dimmed_description.sectors.at(0).floor.nits = 25.0F;
    const auto map = make_map(std::move(description));
    const auto dimmed_map = make_map(std::move(dimmed_description));
    const auto lamp = [](float nits) {
        auto material = glowing(nits, {.r = 1.0F, .g = 0.5F, .b = 0.25F});
        material.emission->texture = 0;
        material.size = Vector2{.x = 1.0F, .y = 1.0F};
        return material;
    };
    const auto pattern = flat_image("pattern", {255, 128, 255, 255}, wulfhilde::ColorSpace::srgb);

    const auto overridden =
        floor_from_above(dimmed_map, assets_of(dimmed_map, {{"floor", lamp(400.0F)}}, {pattern}));
    const auto at_its_own =
        floor_from_above(map, assets_of(map, {{"floor", lamp(25.0F)}}, {pattern}));
    const auto undimmed =
        floor_from_above(map, assets_of(map, {{"floor", lamp(400.0F)}}, {pattern}));

    CAPTURE(describe(overridden), describe(at_its_own), describe(undimmed));
    CHECK(matches(overridden, at_its_own));
    CHECK(undimmed.at(1) > overridden.at(1) + 16);
}

TEST_CASE_METHOD(GpuTest,
                 "a surface's nits apply to its own material alone, so another surface of the "
                 "same material glows at the material's",
                 "[renderer]") {
    auto description = box_map();
    description.ambient.reset();
    description.sectors.at(0).ceiling.material = "floor";
    description.sectors.at(0).ceiling.nits = 10.0F;
    const auto map = make_map(std::move(description));
    auto plain_description = box_map();
    plain_description.ambient.reset();
    const auto plain_map = make_map(std::move(plain_description));

    const auto floor = floor_from_above(map, assets_of(map, {{"floor", glowing(100.0F)}}));
    const auto alone =
        floor_from_above(plain_map, assets_of(plain_map, {{"floor", glowing(100.0F)}}));

    CAPTURE(describe(floor), describe(alone));
    CHECK(matches(floor, alone));
}

TEST_CASE_METHOD(GpuTest,
                 "a normal texture tilts the surface's normal along its material's u and v, as "
                 "its surface lays the material",
                 "[renderer]") {
    // A smooth black floor reflects more of the ambient light the more its normal leans away from
    // the eye, which looks down on it from the west.
    const auto map = make_map(box_map());
    const auto tilted = [](Texel normal, float scale = 1.0F) {
        auto material = plain(black);
        material.roughness.factor = 0.0F;
        material.normal = wulfhilde::NormalProperty{.texture = 0, .scale = scale};
        material.size = Vector2{.x = 1.0F, .y = 1.0F};
        return std::pair{material, flat_image("normal", normal, wulfhilde::ColorSpace::linear)};
    };
    const auto seen = [&map](const std::pair<Material, wulfhilde::TextureImage>& floor,
                             Angle rotation = Angle::degrees(0.0F)) {
        auto description = box_map();
        description.sectors.at(0).floor.rotation = rotation;
        const auto turned = make_map(std::move(description));
        return floor_from_the_west(turned,
                                   assets_of(turned, {{"floor", floor.first}}, {floor.second}));
    };
    // Leaning 60° along +u, along -u, and along +v, as glTF's red, green and blue encode them.
    constexpr Texel toward_plus_u{238, 128, 192, 255};
    constexpr Texel toward_minus_u{17, 128, 192, 255};
    constexpr Texel toward_plus_v{128, 238, 192, 255};
    constexpr Texel straight_up{128, 128, 255, 255};

    const auto flat = floor_from_the_west(
        map, assets_of(map, {{"floor", tilted(straight_up).first}}, {tilted(straight_up).second}));
    const auto away = seen(tilted(toward_plus_u));
    const auto toward = seen(tilted(toward_minus_u));
    const auto across = seen(tilted(toward_plus_v));
    const auto unscaled = seen(tilted(toward_plus_u, 0.0F));
    // Turned a half turn, the material's +u runs along -X, toward the eye.
    const auto turned = seen(tilted(toward_plus_u), Angle::degrees(180.0F));

    CAPTURE(describe(flat), describe(away), describe(toward), describe(across), describe(unscaled),
            describe(turned));
    CHECK(away.at(1) > flat.at(1) + 16);
    CHECK(toward.at(1) + 8 < flat.at(1));
    CHECK(matches(unscaled, flat));
    CHECK(matches(turned, toward));
    // Leaning across the way to the eye, the normal turns from it less than leaning away.
    CHECK(across.at(1) > flat.at(1));
    CHECK(across.at(1) < away.at(1));
}
