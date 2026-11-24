// How the renderer lays materials' textures on surfaces: where u and v run on walls, floors and
// ceilings, how a surface's offset, scale and rotation move a material, and how textures are
// filtered.
#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
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
#include <wulfhilde/renderer/renderer_settings.hpp>
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
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

using Texel = std::array<std::uint8_t, 4>;

constexpr Texel red_texel{200, 20, 20, 255};
constexpr Texel green_texel{20, 200, 20, 255};
constexpr Texel blue_texel{20, 20, 200, 255};
constexpr Texel yellow_texel{200, 200, 20, 255};
constexpr Texel white_texel{255, 255, 255, 255};
constexpr Texel black_texel{0, 0, 0, 255};

// A mip level of texels given row after row, from the top.
wulfhilde::MipLevel level(std::size_t width, std::size_t height, const std::vector<Texel>& texels) {
    REQUIRE(texels.size() == width * height);
    wulfhilde::MipLevel made{.width = width, .height = height};
    for (const auto& texel : texels) {
        made.texels.insert(made.texels.end(), texel.begin(), texel.end());
    }
    return made;
}

// A colour image of the levels given, which make a whole mip chain.
wulfhilde::TextureImage color_image(std::string path, std::vector<wulfhilde::MipLevel> levels) {
    return {
        .path = std::move(path),
        .color_space = wulfhilde::ColorSpace::srgb,
        .levels = std::move(levels),
    };
}

// Four texels: red at the bottom left, green at the bottom right, blue at the top left and yellow
// at the top right.
wulfhilde::TextureImage quadrants() {
    return color_image("quadrants", {
                                        level(2, 2,
                                              {
                                                  blue_texel,
                                                  yellow_texel,
                                                  red_texel,
                                                  green_texel,
                                              }),
                                        level(1, 1, {white_texel}),
                                    });
}

// One row of four texels, from the left: red, green, blue and yellow.
wulfhilde::TextureImage columns() {
    return color_image("columns", {
                                      level(4, 1,
                                            {
                                                red_texel,
                                                green_texel,
                                                blue_texel,
                                                yellow_texel,
                                            }),
                                      level(2, 1, {white_texel, white_texel}),
                                      level(1, 1, {white_texel}),
                                  });
}

// One column of four texels, from the bottom: red, green, blue and yellow.
wulfhilde::TextureImage rows() {
    return color_image("rows", {
                                   level(1, 4,
                                         {
                                             yellow_texel,
                                             blue_texel,
                                             green_texel,
                                             red_texel,
                                         }),
                                   level(1, 2, {white_texel, white_texel}),
                                   level(1, 1, {white_texel}),
                               });
}

// A material that is a texture alone, repeating at a size, under a factor.
wulfhilde::Material textured(std::size_t image, Vector2 size,
                             wulfhilde::Color factor = {.r = 1.0F, .g = 1.0F, .b = 1.0F}) {
    return {
        .color = {.factor = factor, .texture = image},
        .size = size,
    };
}

constexpr wulfhilde::AmbientLight white_light{
    .color = {.r = 1.0F, .g = 1.0F, .b = 1.0F},
    .lux = 250.0F,
};

// The box: a square sector 4 m on a side from (0, 0) to (4, 4), whose surfaces each have a material
// of their own, named after the surface: "floor", "ceiling", and "south", "east", "north" and
// "west" for the walls, each named after the way it faces from outside.
wulfhilde::MapDescription box_map(float floor_height = 0.0F, float ceiling_height = 3.0F) {
    const std::array<std::pair<Vector2, const char*>, 4> walls{
        {
            {{.x = 0.0F, .y = 0.0F}, "south"},
            {{.x = 4.0F, .y = 0.0F}, "east"},
            {{.x = 4.0F, .y = 4.0F}, "north"},
            {{.x = 0.0F, .y = 4.0F}, "west"},
        },
    };
    std::vector<wulfhilde::WallDescription> loop;
    loop.reserve(walls.size());
    for (const auto& [start, material] : walls) {
        loop.push_back({.start = start, .surface = {.material = material}});
    }
    return {
        .ambient = white_light,
        .sectors =
            {
                {
                    .id = "box",
                    .floor = {.height = floor_height, .material = "floor"},
                    .ceiling = {.height = ceiling_height, .material = "ceiling"},
                    .loops = {std::move(loop)},
                },
            },
        .start = {.sector = "box", .point = {.x = 2.0F, .y = 2.0F, .z = floor_height}},
    };
}

// The description of the box's east wall, which runs from (4, 0) to (4, 4).
wulfhilde::SurfaceDescription& east_wall(wulfhilde::MapDescription& description) {
    return description.sectors.at(0).loops.at(0).at(1).surface;
}

wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The assets of the box, every surface in a plain grey but those given materials of their own.
wulfhilde::MapAssets assets_of(const wulfhilde::Map& map,
                               const std::map<std::string, wulfhilde::Material>& materials,
                               std::vector<wulfhilde::TextureImage> images) {
    wulfhilde::MapAssets assets{.images = std::move(images)};
    for (const auto& path : map.materials()) {
        const auto given = materials.find(path);
        assets.materials.push_back(
            given != materials.end()
                ? given->second
                : wulfhilde::Material{.color = {.factor = {.r = 0.2F, .g = 0.2F, .b = 0.2F}}});
    }
    return assets;
}

constexpr wulfhilde::Extent image_extent{.width = 128, .height = 128};

// A camera with a field of view of 90°, in the box.
wulfhilde::Camera camera_at(const wulfhilde::Map& map, Vector3 point, Angle yaw, Angle pitch) {
    return {
        .location = {.sector = map.start().location.sector, .point = point},
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, Angle::degrees(0.0F)),
        .vertical_field_of_view = Angle::degrees(90.0F),
    };
}

wulfhilde::Camera looking_east(const wulfhilde::Map& map, Vector3 point) {
    return camera_at(map, point, Angle::degrees(0.0F), Angle::degrees(0.0F));
}

wulfhilde::Camera looking_down(const wulfhilde::Map& map, Vector3 point) {
    return camera_at(map, point, Angle::degrees(0.0F), Angle::degrees(-90.0F));
}

wulfhilde::Camera looking_up(const wulfhilde::Map& map, Vector3 point) {
    return camera_at(map, point, Angle::degrees(0.0F), Angle::degrees(90.0F));
}

using Pixel = std::array<std::uint8_t, 4>;

// What a camera sees of a point in front of it, in an image it rendered.
class View {
  public:
    View(wulfhilde::Camera camera, wulfhilde::Image image)
        : camera_(camera), image_(std::move(image)) {}

    // The pixel the point lies in, projected as the renderer projects it.
    [[nodiscard]] Pixel at(Vector3 point) const {
        const auto relative = wulfhilde::rotate(wulfhilde::inverse(camera_.orientation),
                                                point - camera_.location.point);
        const auto extent = image_.extent();
        const auto width = static_cast<float>(extent.width);
        const auto height = static_cast<float>(extent.height);
        const auto vertical = 1.0F / std::tan(camera_.vertical_field_of_view.in_radians() / 2.0F);
        const auto horizontal = vertical * height / width;
        // Forward, left and up are x, y and z in the camera's own axes.
        const auto x = (1.0F - (horizontal * relative.y / relative.x)) * width / 2.0F;
        const auto y = (1.0F - (vertical * relative.z / relative.x)) * height / 2.0F;
        REQUIRE(relative.x > 0.0F);
        REQUIRE((x >= 0.0F && x < width && y >= 0.0F && y < height));
        const auto column = static_cast<std::size_t>(x);
        const auto row = static_cast<std::size_t>(y);
        const auto start = ((row * extent.width) + column) * wulfhilde::Image::bytes_per_pixel;
        Pixel pixel{};
        std::ranges::copy(image_.pixels().subspan(start, pixel.size()), pixel.begin());
        return pixel;
    }

  private:
    wulfhilde::Camera camera_;
    wulfhilde::Image image_;
};

View view(wulfhilde::Renderer& renderer, const wulfhilde::Camera& camera) {
    auto image = renderer.render_image(camera, image_extent);
    require_value(image);
    return {camera, *std::move(image)};
}

wulfhilde::Renderer renderer_of(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets,
                                wulfhilde::RendererSettings settings = gpu_test_settings) {
    auto renderer = wulfhilde::Renderer::create_headless(settings);
    require_value(renderer);
    require_value(renderer->set_map(map, assets));
    return *std::move(renderer);
}

// Whether a pixel's channels that `channels` names are each well above every other, as a surface
// of one of the test colours shows under white light.
bool shows(const Pixel& pixel, std::array<bool, 3> channels) {
    constexpr int margin = 64;
    for (std::size_t strong = 0; strong < 3; ++strong) {
        for (std::size_t weak = 0; weak < 3; ++weak) {
            if (channels.at(strong) && !channels.at(weak) &&
                pixel.at(strong) < pixel.at(weak) + margin) {
                return false;
            }
        }
    }
    return true;
}

constexpr std::array red{true, false, false};
constexpr std::array green{false, true, false};
constexpr std::array blue{false, false, true};
constexpr std::array yellow{true, true, false};

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

// A view down from a point 1.5 m above the box's floor, onto a floor with one material.
View floor_view(const wulfhilde::MapDescription& description, const wulfhilde::Material& material,
                const wulfhilde::TextureImage& image, Vector2 above) {
    const auto map = make_map(description);
    auto renderer = renderer_of(map, assets_of(map, {{"floor", material}}, {image}));
    return view(renderer, looking_down(map, {.x = above.x, .y = above.y, .z = 1.5F}));
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a wall's texture stands upright, running to the right from the wall's start, "
                 "with height as v",
                 "[renderer]") {
    // The floor is raised, to show that v is height above zero, not above the floor. Seen from
    // inside, the east wall's start, at y = 0, is on the right.
    auto description = box_map(0.75F, 3.75F);
    east_wall(description).material = "quadrants";
    const auto map = make_map(std::move(description));
    auto renderer = renderer_of(
        map, assets_of(map, {{"quadrants", textured(0, {.x = 3.0F, .y = 1.5F})}}, {quadrants()}));

    const auto seen = view(renderer, looking_east(map, {.x = 0.5F, .y = 2.0F, .z = 2.25F}));
    const auto bottom_left = seen.at({.x = 4.0F, .y = 2.25F, .z = 1.875F});
    const auto bottom_right = seen.at({.x = 4.0F, .y = 0.75F, .z = 1.875F});
    const auto top_left = seen.at({.x = 4.0F, .y = 2.25F, .z = 2.625F});
    const auto top_right = seen.at({.x = 4.0F, .y = 0.75F, .z = 2.625F});

    CAPTURE(describe(bottom_left), describe(bottom_right), describe(top_left), describe(top_right));
    CHECK(shows(bottom_left, red));
    CHECK(shows(bottom_right, green));
    CHECK(shows(top_left, blue));
    CHECK(shows(top_right, yellow));
}

TEST_CASE_METHOD(GpuTest, "a floor's texture and a ceiling's lie along X and Y", "[renderer]") {
    auto description = box_map();
    description.sectors.at(0).floor.material = "quadrants";
    description.sectors.at(0).ceiling.material = "quadrants";
    const auto map = make_map(std::move(description));
    auto renderer = renderer_of(
        map, assets_of(map, {{"quadrants", textured(0, {.x = 2.0F, .y = 2.0F})}}, {quadrants()}));
    const Vector3 eye{.x = 1.0F, .y = 1.0F, .z = 1.5F};

    for (const auto& [seen, height] : {
             std::pair{view(renderer, looking_down(map, eye)), 0.0F},
             std::pair{view(renderer, looking_up(map, eye)), 3.0F},
         }) {
        CAPTURE(height);
        const auto low_x_low_y = seen.at({.x = 0.5F, .y = 0.5F, .z = height});
        const auto high_x_low_y = seen.at({.x = 1.5F, .y = 0.5F, .z = height});
        const auto low_x_high_y = seen.at({.x = 0.5F, .y = 1.5F, .z = height});
        const auto high_x_high_y = seen.at({.x = 1.5F, .y = 1.5F, .z = height});

        CAPTURE(describe(low_x_low_y), describe(high_x_low_y), describe(low_x_high_y),
                describe(high_x_high_y));
        CHECK(shows(low_x_low_y, red));
        CHECK(shows(high_x_low_y, green));
        CHECK(shows(low_x_high_y, blue));
        CHECK(shows(high_x_high_y, yellow));
    }
}

TEST_CASE_METHOD(GpuTest, "a surface's offset moves its material along +u and +v, in metres",
                 "[renderer]") {
    SECTION("along a wall") {
        // Without the offset, the red column would cover the metre from y = 3 to y = 4.
        auto description = box_map();
        east_wall(description).material = "columns";
        east_wall(description).offset = {.x = 1.0F, .y = 0.0F};
        const auto map = make_map(std::move(description));
        auto renderer = renderer_of(
            map, assets_of(map, {{"columns", textured(0, {.x = 4.0F, .y = 1.0F})}}, {columns()}));

        const auto seen = view(renderer, looking_east(map, {.x = 0.5F, .y = 2.0F, .z = 1.5F}));
        const auto behind = seen.at({.x = 4.0F, .y = 3.5F, .z = 1.5F});
        const auto moved_to = seen.at({.x = 4.0F, .y = 2.5F, .z = 1.5F});
        const auto after = seen.at({.x = 4.0F, .y = 1.5F, .z = 1.5F});

        CAPTURE(describe(behind), describe(moved_to), describe(after));
        CHECK(shows(behind, yellow));
        CHECK(shows(moved_to, red));
        CHECK(shows(after, green));
    }
    SECTION("across a floor") {
        // Without the offset, the red row would cover the metre from y = 0 to y = 1.
        auto description = box_map();
        description.sectors.at(0).floor.offset = {.x = 0.0F, .y = 1.0F};
        const auto seen = floor_view(description, textured(0, {.x = 1.0F, .y = 4.0F}), rows(),
                                     {.x = 2.0F, .y = 2.5F});
        const auto moved_to = seen.at({.x = 2.0F, .y = 1.5F, .z = 0.0F});
        const auto after = seen.at({.x = 2.0F, .y = 2.5F, .z = 0.0F});
        const auto last = seen.at({.x = 2.0F, .y = 3.5F, .z = 0.0F});

        CAPTURE(describe(moved_to), describe(after), describe(last));
        CHECK(shows(moved_to, red));
        CHECK(shows(after, green));
        CHECK(shows(last, blue));
    }
}

TEST_CASE_METHOD(GpuTest, "a surface's scale multiplies its material's size, and textures repeat",
                 "[renderer]") {
    // At half the size, the four columns each cover a quarter of 2 m, and then repeat.
    auto description = box_map();
    description.sectors.at(0).floor.scale = {.x = 0.5F, .y = 1.0F};
    const auto seen = floor_view(description, textured(0, {.x = 4.0F, .y = 1.0F}), columns(),
                                 {.x = 1.25F, .y = 2.0F});
    const auto second = seen.at({.x = 0.75F, .y = 2.0F, .z = 0.0F});
    const auto third = seen.at({.x = 1.25F, .y = 2.0F, .z = 0.0F});
    const auto repeated = seen.at({.x = 2.25F, .y = 2.0F, .z = 0.0F});

    CAPTURE(describe(second), describe(third), describe(repeated));
    CHECK(shows(second, green));
    CHECK(shows(third, blue));
    CHECK(shows(repeated, red));
}

TEST_CASE_METHOD(GpuTest, "a surface's rotation turns its material from +u toward +v",
                 "[renderer]") {
    // Turned a quarter, what lay along +u from the red texel lies along +v from it.
    auto description = box_map();
    description.sectors.at(0).floor.rotation = Angle::degrees(90.0F);
    const auto seen = floor_view(description, textured(0, {.x = 2.0F, .y = 2.0F}), quadrants(),
                                 {.x = 1.0F, .y = 1.0F});
    const auto origin = seen.at({.x = 1.5F, .y = 0.5F, .z = 0.0F});
    const auto along_y = seen.at({.x = 1.5F, .y = 1.5F, .z = 0.0F});
    const auto along_x = seen.at({.x = 0.5F, .y = 0.5F, .z = 0.0F});
    const auto across = seen.at({.x = 0.5F, .y = 1.5F, .z = 0.0F});

    CAPTURE(describe(origin), describe(along_y), describe(along_x), describe(across));
    CHECK(shows(origin, red));
    CHECK(shows(along_y, green));
    CHECK(shows(along_x, blue));
    CHECK(shows(across, yellow));
}

TEST_CASE_METHOD(GpuTest,
                 "a material turns about the point its offset moves it to, and scales along its "
                 "own turned axes",
                 "[renderer]") {
    // The material's corner is moved to x = 1, and turned a quarter, so its u runs along +Y and
    // its v along -X, and its size along its own u is halved to 1 m.
    auto description = box_map();
    auto& floor = description.sectors.at(0).floor;
    floor.offset = {.x = 1.0F, .y = 0.0F};
    floor.rotation = Angle::degrees(90.0F);
    floor.scale = {.x = 0.5F, .y = 1.0F};
    const auto seen = floor_view(description, textured(0, {.x = 2.0F, .y = 2.0F}), quadrants(),
                                 {.x = 1.0F, .y = 1.0F});
    const auto bottom_left = seen.at({.x = 0.5F, .y = 0.25F, .z = 0.0F});
    const auto bottom_right = seen.at({.x = 0.5F, .y = 0.75F, .z = 0.0F});
    const auto top_left = seen.at({.x = 1.5F, .y = 0.25F, .z = 0.0F});
    const auto top_right = seen.at({.x = 1.5F, .y = 0.75F, .z = 0.0F});
    const auto repeated = seen.at({.x = 0.5F, .y = 1.25F, .z = 0.0F});

    CAPTURE(describe(bottom_left), describe(bottom_right), describe(top_left), describe(top_right),
            describe(repeated));
    CHECK(shows(bottom_left, red));
    CHECK(shows(bottom_right, green));
    CHECK(shows(top_left, blue));
    CHECK(shows(top_right, yellow));
    CHECK(shows(repeated, red));
}

TEST_CASE_METHOD(GpuTest, "a texture's colour is multiplied by its material's factor",
                 "[renderer]") {
    // Magenta takes the green out of yellow, leaving red.
    const auto seen = floor_view(
        box_map(), textured(0, {.x = 2.0F, .y = 2.0F}, {.r = 1.0F, .g = 0.02F, .b = 1.0F}),
        quadrants(), {.x = 1.0F, .y = 1.0F});
    const auto yellow_texel_seen = seen.at({.x = 1.5F, .y = 1.5F, .z = 0.0F});

    CAPTURE(describe(yellow_texel_seen));
    CHECK(shows(yellow_texel_seen, red));
}

TEST_CASE_METHOD(GpuTest,
                 "every level of a texture's mip chain is uploaded, and a texture seen far "
                 "smaller than it is reads its smallest",
                 "[renderer]") {
    // Each level is a colour of its own, which no filtering of the level above would give.
    const auto image = color_image("levels", {
                                                 level(4, 4, std::vector(16, red_texel)),
                                                 level(2, 2, std::vector(4, green_texel)),
                                                 level(1, 1, {blue_texel}),
                                             });
    const Vector2 above{.x = 2.0F, .y = 2.0F};

    const auto near = floor_view(box_map(), textured(0, {.x = 4.0F, .y = 4.0F}), image, above)
                          .at({.x = 2.0F, .y = 2.0F, .z = 0.0F});
    const auto far = floor_view(box_map(), textured(0, {.x = 0.01F, .y = 0.01F}), image, above)
                         .at({.x = 2.0F, .y = 2.0F, .z = 0.0F});

    CAPTURE(describe(near), describe(far));
    CHECK(shows(near, red));
    CHECK(shows(far, blue));
}

TEST_CASE_METHOD(GpuTest, "the textures of a map set in place of another are drawn instead",
                 "[renderer]") {
    const auto map = make_map(box_map());
    const auto flat = [&map](Texel texel) {
        return assets_of(map, {{"floor", textured(0, {.x = 1.0F, .y = 1.0F})}},
                         {color_image("flat", {level(1, 1, {texel})})});
    };
    auto renderer = renderer_of(map, flat(red_texel));
    const auto camera = looking_down(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F});
    const auto before = view(renderer, camera).at({.x = 2.0F, .y = 2.0F, .z = 0.0F});

    require_value(renderer.set_map(map, flat(blue_texel)));
    const auto after = view(renderer, camera).at({.x = 2.0F, .y = 2.0F, .z = 0.0F});

    CAPTURE(describe(before), describe(after));
    CHECK(shows(before, red));
    CHECK(shows(after, blue));
}

TEST_CASE_METHOD(GpuTest, "a renderer comes up and draws at every anisotropy from 1 to 16",
                 "[renderer]") {
    const auto map = make_map(box_map());
    const auto assets =
        assets_of(map, {{"floor", textured(0, {.x = 2.0F, .y = 2.0F})}}, {quadrants()});

    for (std::size_t anisotropy = 1; anisotropy <= 16; ++anisotropy) {
        CAPTURE(anisotropy);
        auto renderer = renderer_of(map, assets, {.validation = true, .anisotropy = anisotropy});
        const auto seen = view(renderer, looking_down(map, {.x = 1.0F, .y = 1.0F, .z = 1.5F}));

        CHECK(shows(seen.at({.x = 0.5F, .y = 0.5F, .z = 0.0F}), red));
    }
}

TEST_CASE_METHOD(GpuTest,
                 "anisotropic filtering keeps the detail of a floor seen at a grazing angle, "
                 "which trilinear filtering blurs",
                 "[renderer]") {
    // A corridor 64 m long, whose floor has stripes across Y, which recede into the distance
    // along X. Seen low along it, each pixel spans far more of the floor along X than across it.
    auto description = box_map();
    auto& loop = description.sectors.at(0).loops.at(0);
    loop.at(1).start = {.x = 64.0F, .y = 0.0F};
    loop.at(2).start = {.x = 64.0F, .y = 4.0F};
    const auto map = make_map(std::move(description));
    std::vector<Texel> striped;
    for (std::size_t row = 0; row < 8; ++row) {
        for (std::size_t column = 0; column < 8; ++column) {
            striped.push_back((row / 2) % 2 == 0 ? white_texel : black_texel);
        }
    }
    const auto image = color_image("stripes", {
                                                  level(8, 8, striped),
                                                  level(4, 4, std::vector(16, white_texel)),
                                                  level(2, 2, std::vector(4, white_texel)),
                                                  level(1, 1, {white_texel}),
                                              });
    // The coarser levels are white, so whatever reads them loses the stripes.
    const auto assets = assets_of(map, {{"floor", textured(0, {.x = 1.0F, .y = 1.0F})}}, {image});
    const auto contrast = [&](std::size_t anisotropy) {
        auto renderer = renderer_of(map, assets, {.validation = true, .anisotropy = anisotropy});
        const auto seen = view(renderer, looking_east(map, {.x = 0.5F, .y = 2.0F, .z = 0.5F}));
        // Across the floor 6 m ahead, where a pixel spans about 0.6 m along X and 0.1 m across.
        int darkest = 255;
        int lightest = 0;
        for (std::size_t step = 0; step <= 40; ++step) {
            const auto y = 1.0F + (0.05F * static_cast<float>(step));
            const auto pixel = seen.at({.x = 6.5F, .y = y, .z = 0.0F});
            darkest = std::min<int>(darkest, pixel.at(1));
            lightest = std::max<int>(lightest, pixel.at(1));
        }
        return lightest - darkest;
    };

    const auto trilinear = contrast(1);
    const auto anisotropic = contrast(16);

    CAPTURE(trilinear, anisotropic);
    CHECK(anisotropic > 2 * trilinear);
}
