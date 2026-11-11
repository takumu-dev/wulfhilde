#include <wulfhilde/assets/load_map_assets.hpp>

#include "test_files.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/asset_error.hpp>
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <lodepng.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using wulfhilde::AssetProblemCode;
using wulfhilde::Channel;
using wulfhilde::Color;
using wulfhilde::ColorSpace;
using wulfhilde::ContentRoot;
using wulfhilde::load_map_assets;
using wulfhilde::Map;
using wulfhilde::MapAssets;
using wulfhilde::Material;
using wulfhilde::MipLevel;

namespace {

// The content root of the materials tests.
ContentRoot materials_root() { return open_root(test_files() / "materials"); }

// A map of one square sector, whose floor, ceiling and walls carry the materials given, the walls
// taking the last of them in turn.
Map map_of(const std::string& floor, const std::string& ceiling, const std::string& wall) {
    const auto surface = [&wall](float x, float y) {
        return wulfhilde::WallDescription{.start = {.x = x, .y = y}, .surface = {.material = wall}};
    };
    auto map = Map::create({
        .sectors =
            {
                {
                    .id = "room",
                    .floor = {.height = 0.0F, .material = floor},
                    .ceiling = {.height = 3.0F, .material = ceiling},
                    .loops = {{surface(0, 0), surface(4, 0), surface(4, 4), surface(0, 4)}},
                },
            },
        .start = {.sector = "room", .point = {.x = 2.0F, .y = 2.0F, .z = 0.0F}},
    });
    REQUIRE(map.has_value());
    return std::move(*map);
}

// A map whose every surface carries one material.
Map map_of(const std::string& material) { return map_of(material, material, material); }

MapAssets load_valid(const ContentRoot& root, const Map& map) {
    auto assets = load_map_assets(root, map);
    if (!assets) {
        FAIL(std::format("{}", assets.error()));
    }
    return std::move(*assets);
}

// Writes a material whose colour reads one texture, and whose other properties are plain.
void write_color_material(const std::filesystem::path& directory, std::string_view name,
                          std::string_view texture) {
    write_text(directory / std::format("{}.material.json", name), std::format(R"({{
  "format": "wulfhilde-material", "version": 1,
  "color": {{ "texture": "{}" }},
  "roughness": {{ "factor": 1 }},
  "metallic": {{ "factor": 0 }},
  "size": [1, 1]
}})",
                                                                              texture));
}

// Writes a material whose every property is plain but `property`, which reads one texture.
void write_material_reading(const std::filesystem::path& directory, std::string_view material,
                            std::string_view property, std::string_view texture) {
    const auto plain = [&](std::string_view name, std::string_view value) {
        return name == property ? std::format(R"("{}": {{ "texture": "{}" }})", name, texture)
                                : std::format(R"("{}": {})", name, value);
    };
    auto members = std::format(R"("format": "wulfhilde-material", "version": 1, {}, {}, {}, )"
                               R"("size": [1, 1])",
                               plain("color", R"({ "factor": [1, 1, 1] })"),
                               plain("roughness", R"({ "factor": 1 })"),
                               plain("metallic", R"({ "factor": 0 })"));
    if (property == "normal") {
        members += std::format(R"(, "normal": {{ "texture": "{}" }})", texture);
    }
    write_text(directory / std::format("{}.material.json", material),
               std::format("{{ {} }}", members));
}

// Writes a PNG of 8-bit red, green and blue, rows from the top.
void write_rgb_png(const std::filesystem::path& path, unsigned width, unsigned height,
                   const std::vector<unsigned char>& pixels) {
    REQUIRE(lodepng::encode(path.string(), pixels, width, height, LCT_RGB, 8) == 0);
}

// Makes a colour mode a palette of two colours, as 8-bit indices.
void give_palette(LodePNGColorMode& color) {
    color.colortype = LCT_PALETTE;
    color.bitdepth = 8;
    REQUIRE(lodepng_palette_add(&color, 200, 100, 50, 255) == 0);
    REQUIRE(lodepng_palette_add(&color, 10, 20, 30, 255) == 0);
}

// The texels of a mip level, as red, green, blue and alpha.
std::vector<std::uint8_t> texels(std::initializer_list<std::uint8_t> values) { return values; }

} // namespace

TEST_CASE("a material without textures loads as the material its file describes", "[assets]") {
    const auto assets = load_valid(materials_root(), map_of("valid/plain"));

    CHECK(assets.images.empty());
    REQUIRE(assets.materials.size() == 1);
    CHECK(assets.materials.at(0) == Material{
                                        .color = {.factor = {.r = 0.8F, .g = 0.5F, .b = 0.25F}},
                                        .roughness = {.factor = 0.75F},
                                        .metallic = {.factor = 0.0F},
                                    });
}

TEST_CASE("each invalid material reports exactly the problems written beside it", "[assets]") {
    const auto root = materials_root();
    const auto invalid = test_files() / "materials" / "invalid";
    const auto materials = asset_paths(invalid, wulfhilde::material_extension);
    CHECK(materials == asset_paths(invalid, ".expected"));
    CHECK(materials.size() >= 30);

    for (const auto& path : materials) {
        CAPTURE(path);
        const auto assets = load_map_assets(root, map_of("invalid/" + path));
        REQUIRE(!assets.has_value());
        CHECK(std::format("{}\n", assets.error()) == read_text(invalid / (path + ".expected")));
    }
}

TEST_CASE("a material with textures loads with each of them as an image in the way it is read",
          "[assets]") {
    const auto assets = load_valid(materials_root(), map_of("valid/everything"));

    REQUIRE(assets.materials.size() == 1);
    CHECK(assets.materials.at(0) ==
          Material{
              .color = {.factor = {.r = 1.0F, .g = 0.5F, .b = 0.25F}, .texture = 0},
              .roughness = {.factor = 1.0F, .texture = 1, .channel = Channel::g},
              .metallic = {.factor = 0.5F, .texture = 1, .channel = Channel::b},
              .normal = wulfhilde::NormalProperty{.texture = 2, .scale = 0.5F},
              .occlusion = wulfhilde::OcclusionProperty{.value = {.texture = 3}, .strength = 0.75F},
              .emission =
                  wulfhilde::Emission{
                      .nits = 100.0F,
                      .color = Color::from_temperature(2'700.0F),
                      .texture = 0,
                  },
              .size = wulfhilde::Vector2{.x = 2.0F, .y = 1.0F},
              .properties = R"({ "glows": true })",
          });

    REQUIRE(assets.images.size() == 4);
    CHECK(assets.images.at(0).path == "textures/rgb");
    CHECK(assets.images.at(0).color_space == ColorSpace::srgb);
    CHECK(assets.images.at(1).path == "textures/rgb");
    CHECK(assets.images.at(1).color_space == ColorSpace::linear);
    CHECK(assets.images.at(2).path == "textures/rgb");
    CHECK(assets.images.at(2).color_space == ColorSpace::linear);
    CHECK(assets.images.at(3).path == "textures/grey");
    CHECK(assets.images.at(3).color_space == ColorSpace::linear);
    // The PNG's rows run from the top: black then white, then white then black.
    CHECK(
        assets.images.at(3).levels.at(0) ==
        MipLevel{
            .width = 2,
            .height = 2,
            .texels = texels({0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255}),
        });
}

TEST_CASE("an emission may give its colour, and an occlusion only a factor", "[assets]") {
    const auto assets = load_valid(materials_root(), map_of("valid/glow"));

    REQUIRE(assets.materials.size() == 1);
    const auto& material = assets.materials.at(0);
    CHECK(material.emission == wulfhilde::Emission{
                                   .nits = 250.0F,
                                   .color = {.r = 1.0F, .g = 0.75F, .b = 0.5F},
                                   .texture = 0,
                               });
    CHECK(material.occlusion == wulfhilde::OcclusionProperty{.value = {.factor = 0.5F}});
    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).color_space == ColorSpace::srgb);
}

TEST_CASE("the materials are held by the IDs the map gives them", "[assets]") {
    const auto assets =
        load_valid(materials_root(), map_of("valid/glow", "valid/plain", "valid/glow"));

    REQUIRE(assets.materials.size() == 2);
    CHECK(assets.materials.at(0).emission.has_value());
    CHECK(assets.materials.at(1).color.factor == Color{.r = 0.8F, .g = 0.5F, .b = 0.25F});
}

TEST_CASE("a missing material is reported once, at the first surface that carries it", "[assets]") {
    const auto assets =
        load_map_assets(materials_root(), map_of("valid/plain", "missing/ceiling", "missing/wall"));

    REQUIRE(!assets.has_value());
    CHECK(assets.error().code == wulfhilde::AssetErrorCode::asset_problems);
    REQUIRE(assets.error().problems.size() == 2);
    CHECK(assets.error().problems.at(0).code == AssetProblemCode::missing_file);
    CHECK(std::format("{}", assets.error()) ==
          "there is no file at \"missing/ceiling.material.json\" (/sectors/0/ceiling/material)\n"
          "there is no file at \"missing/wall.material.json\" "
          "(/sectors/0/loops/0/0/surface/material)");
}

TEST_CASE("every problem in every material is reported at once", "[assets]") {
    const auto assets = load_map_assets(
        materials_root(), map_of("invalid/wrong_type", "valid/plain", "invalid/missing_size"));

    REQUIRE(!assets.has_value());
    CHECK(std::format("{}\n", assets.error()) ==
          read_text(test_files() / "materials" / "invalid" / "wrong_type.expected") +
              read_text(test_files() / "materials" / "invalid" / "missing_size.expected"));
}

TEST_CASE("a missing image that two materials read is reported once, at the first", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_shared_problem");
    write_color_material(directory, "first", "nothing");
    write_color_material(directory, "second", "nothing");

    const auto assets = load_map_assets(open_root(directory), map_of("first", "second", "first"));

    REQUIRE(!assets.has_value());
    CHECK(std::format("{}", assets.error()) ==
          "first.material.json:3:25: there is no file at \"nothing.png\" (/color/texture)");
}

TEST_CASE("an image that two materials read the same way is held once", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_shared_image");
    write_rgb_png(directory / "bricks.png", 1, 1, {10, 20, 30});
    write_color_material(directory, "first", "bricks");
    write_color_material(directory, "second", "bricks");

    const auto assets = load_valid(open_root(directory), map_of("first", "second", "second"));

    CHECK(assets.images.size() == 1);
    CHECK(assets.materials.at(1).color.texture == 0);
}

TEST_CASE("a texture's mip chain halves each level, rounding down, to one texel", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_mip_sizes");
    write_rgb_png(directory / "odd.png", 5, 3,
                  std::vector<unsigned char>(std::size_t{5} * 3 * 3, 128));
    write_color_material(directory, "odd", "odd");

    const auto assets = load_valid(open_root(directory), map_of("odd"));

    REQUIRE(assets.images.size() == 1);
    const auto& levels = assets.images.at(0).levels;
    REQUIRE(levels.size() == 3);
    CHECK(levels.at(1).width == 2);
    CHECK(levels.at(1).height == 1);
    CHECK(levels.at(1).texels == texels({128, 128, 128, 255, 128, 128, 128, 255}));
    CHECK(levels.at(2).width == 1);
    CHECK(levels.at(2).height == 1);
    CHECK(levels.at(2).texels == texels({128, 128, 128, 255}));
}

TEST_CASE("a colour texture's mips average in linear light, and a value texture's as they are",
          "[assets]") {
    const auto directory = scratch_directory("load_map_assets_mip_light");
    write_rgb_png(directory / "half.png", 2, 1, {0, 0, 0, 255, 255, 255});
    write_material_reading(directory, "colored", "color", "half");
    write_material_reading(directory, "rough", "roughness", "half");

    const auto assets = load_valid(open_root(directory), map_of("colored", "rough", "rough"));

    REQUIRE(assets.images.size() == 2);
    // Half of white's light is 0.5 linear, which sRGB encodes as 188.
    CHECK(assets.images.at(0).levels.at(1).texels == texels({188, 188, 188, 255}));
    CHECK(assets.images.at(1).levels.at(1).texels == texels({128, 128, 128, 255}));
}

TEST_CASE("a normal texture's mips are renormalised", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_mip_normals");
    // +u and straight out of the surface.
    write_rgb_png(directory / "bend.png", 2, 1, {255, 128, 128, 128, 128, 255});
    write_material_reading(directory, "bent", "normal", "bend");

    const auto assets = load_valid(open_root(directory), map_of("bent"));

    REQUIRE(assets.images.size() == 1);
    // Halfway between them, at unit length: (0.71, 0.01, 0.71), where the plain average would be
    // half as long.
    CHECK(assets.images.at(0).levels.at(1).texels == texels({218, 128, 218, 255}));
}

TEST_CASE("an image of 16 bits per channel is narrowed to 8", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_sixteen_bits");
    // Big-endian grey: 0xABCD, of which 8 bits keep 171.
    REQUIRE(lodepng::encode((directory / "deep.png").string(),
                            std::vector<unsigned char>{0xAB, 0xCD}, 1, 1, LCT_GREY, 16) == 0);
    write_material_reading(directory, "deep", "roughness", "deep");

    const auto assets = load_valid(open_root(directory), map_of("deep"));

    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).levels.at(0).texels == texels({171, 171, 171, 255}));
}

TEST_CASE("a palette image is read as the colours of its palette", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_palette");
    lodepng::State state;
    give_palette(state.info_png.color);
    give_palette(state.info_raw);
    state.encoder.auto_convert = 0;
    std::vector<unsigned char> png;
    REQUIRE(lodepng::encode(png, std::vector<unsigned char>{1, 0}, 2, 1, state) == 0);
    REQUIRE(lodepng::save_file(png, (directory / "indexed.png").string()) == 0);
    write_color_material(directory, "indexed", "indexed");

    const auto assets = load_valid(open_root(directory), map_of("indexed"));

    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).levels.at(0).texels == texels({10, 20, 30, 255, 200, 100, 50, 255}));
}

TEST_CASE("an image may be 4,096 pixels on a side", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_largest_image");
    write_rgb_png(directory / "wide.png", 4'096, 1,
                  std::vector<unsigned char>(std::size_t{4'096} * 3, 64));
    write_color_material(directory, "wide", "wide");

    const auto assets = load_valid(open_root(directory), map_of("wide"));

    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).levels.at(0).width == 4'096);
    CHECK(assets.images.at(0).levels.size() == 13);
}

TEST_CASE("an image of fewer than 8 bits per channel is widened to 8", "[assets]") {
    const auto directory = scratch_directory("load_map_assets_one_bit");
    // One bit of grey for each pixel, packed: white, then black.
    REQUIRE(lodepng::encode((directory / "mask.png").string(), std::vector<unsigned char>{0x80}, 2,
                            1, LCT_GREY, 1) == 0);
    write_material_reading(directory, "masked", "roughness", "mask");

    const auto assets = load_valid(open_root(directory), map_of("masked"));

    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).levels.at(0).texels == texels({255, 255, 255, 255, 0, 0, 0, 255}));
}

TEST_CASE("an image's colour space comes from the property that reads it, never its metadata",
          "[assets]") {
    const auto directory = scratch_directory("load_map_assets_metadata");
    // The file says its values are linear, and the colour still reads them as sRGB.
    lodepng::State state;
    state.info_raw.colortype = LCT_RGB;
    state.info_png.color.colortype = LCT_RGB;
    state.info_png.gama_defined = 1;
    state.info_png.gama_gamma = 100'000;
    state.encoder.auto_convert = 0;
    std::vector<unsigned char> png;
    REQUIRE(lodepng::encode(png, std::vector<unsigned char>{0, 0, 0, 255, 255, 255}, 2, 1, state) ==
            0);
    REQUIRE(lodepng::save_file(png, (directory / "linear.png").string()) == 0);
    write_color_material(directory, "linear", "linear");

    const auto assets = load_valid(open_root(directory), map_of("linear"));

    REQUIRE(assets.images.size() == 1);
    CHECK(assets.images.at(0).color_space == ColorSpace::srgb);
    CHECK(assets.images.at(0).levels.at(0).texels == texels({0, 0, 0, 255, 255, 255, 255, 255}));
    CHECK(assets.images.at(0).levels.at(1).texels == texels({188, 188, 188, 255}));
}
