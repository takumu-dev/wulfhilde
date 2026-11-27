// How the renderer draws skies: each shows its backdrop infinitely far away, in the direction it
// is seen in its sector's coordinates, at its own brightness or else the backdrop's.
#include "gpu_test.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector3;

using Texel = std::array<std::uint8_t, 4>;
using Pixel = std::array<std::uint8_t, 4>;

constexpr Texel red_texel{200, 20, 20, 255};
constexpr Texel green_texel{20, 200, 20, 255};
constexpr Texel blue_texel{20, 20, 200, 255};
constexpr Texel yellow_texel{200, 200, 20, 255};
constexpr Texel white_texel{255, 255, 255, 255};

// A mip level of texels given row after row, from the top.
wulfhilde::MipLevel level(std::size_t width, std::size_t height, const std::vector<Texel>& texels) {
    REQUIRE(texels.size() == width * height);
    wulfhilde::MipLevel made{.width = width, .height = height};
    for (const auto& texel : texels) {
        made.texels.insert(made.texels.end(), texel.begin(), texel.end());
    }
    return made;
}

// An equirectangular image of four columns and two rows, each column a quarter turn about Z and
// each row a hemisphere. Its centre faces +X, so the columns from the left cover the turns toward
// +Y and -X, +X and +Y, -Y and +X, and -X and -Y. The upper hemisphere shows, from the left,
// blue, red, green and yellow; the lower one swaps each pair.
wulfhilde::TextureImage quarters() {
    return {
        .path = "quarters",
        .color_space = wulfhilde::ColorSpace::srgb,
        .levels =
            {
                level(4, 2,
                      {
                          blue_texel,
                          red_texel,
                          green_texel,
                          yellow_texel,
                          red_texel,
                          blue_texel,
                          yellow_texel,
                          green_texel,
                      }),
                level(2, 1, {white_texel, white_texel}),
                level(1, 1, {white_texel}),
            },
    };
}

// An equirectangular image of eight columns and four rows, each texel a colour of its own.
wulfhilde::TextureImage patchwork() {
    std::vector<Texel> texels;
    for (std::uint8_t row = 0; row < 4; ++row) {
        for (std::uint8_t column = 0; column < 8; ++column) {
            texels.push_back({
                static_cast<std::uint8_t>(30 * column),
                static_cast<std::uint8_t>(60 * row),
                static_cast<std::uint8_t>(240 - (25 * column)),
                255,
            });
        }
    }
    return {
        .path = "patchwork",
        .color_space = wulfhilde::ColorSpace::srgb,
        .levels =
            {
                level(8, 4, texels),
                level(4, 2, std::vector<Texel>(8, white_texel)),
                level(2, 1, {white_texel, white_texel}),
                level(1, 1, {white_texel}),
            },
    };
}

// One white texel.
wulfhilde::TextureImage white() {
    return {
        .path = "white",
        .color_space = wulfhilde::ColorSpace::srgb,
        .levels = {level(1, 1, {white_texel})},
    };
}

// The yard: a square sector 4 m on a side from (0, 0) to (4, 4), with grey walls, whose floor at
// 0 m shows the backdrop "below" and whose ceiling at 3 m shows "above", at the nits given or else
// the backdrop's.
wulfhilde::Map yard_map(std::optional<float> ceiling_nits = std::nullopt) {
    std::vector<wulfhilde::WallDescription> walls;
    for (const auto& [x, y] : {std::pair{0.0F, 0.0F}, {4.0F, 0.0F}, {4.0F, 4.0F}, {0.0F, 4.0F}}) {
        walls.push_back({.start = {.x = x, .y = y}, .surface = {.material = "grey"}});
    }
    wulfhilde::MapDescription description{
        .ambient =
            wulfhilde::AmbientLight{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 250.0F},
        .start = {.sector = "yard", .point = {.x = 2.0F, .y = 2.0F, .z = 0.0F}},
    };
    auto& yard = description.sectors.emplace_back();
    yard.id = "yard";
    yard.floor = {.height = 0.0F, .sky = wulfhilde::SkyDescription{.backdrop = "below"}};
    yard.ceiling = {
        .height = 3.0F,
        .sky = wulfhilde::SkyDescription{.backdrop = "above", .nits = ceiling_nits},
    };
    yard.loops.push_back(std::move(walls));
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The yard's assets: grey walls, and both its skies showing one image at 20 nits, a little brighter
// than a mid-grey surface shows.
wulfhilde::MapAssets yard_assets(wulfhilde::TextureImage image) {
    return {
        .materials = {{.color = {.factor = {.r = 0.2F, .g = 0.2F, .b = 0.2F}}}},
        .backdrops = {{.nits = 20.0F, .image = 0}, {.nits = 20.0F, .image = 0}},
        .images = {std::move(image)},
    };
}

constexpr wulfhilde::Extent image_extent{.width = 128, .height = 128};

wulfhilde::Camera camera_at(Vector3 point, Angle yaw, Angle pitch, Angle roll = Angle{}) {
    return {
        .location = {.sector = wulfhilde::SectorId{0}, .point = point},
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, roll),
        .vertical_field_of_view = Angle::degrees(90.0F),
    };
}

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
        return pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
    }

    // The pixel at the centre of the image, where the camera looks.
    [[nodiscard]] Pixel centre() const {
        return pixel(image_.extent().width / 2, image_.extent().height / 2);
    }

  private:
    [[nodiscard]] Pixel pixel(std::size_t column, std::size_t row) const {
        const auto start =
            ((row * image_.extent().width) + column) * wulfhilde::Image::bytes_per_pixel;
        Pixel found{};
        std::ranges::copy(image_.pixels().subspan(start, found.size()), found.begin());
        return found;
    }

    wulfhilde::Camera camera_;
    wulfhilde::Image image_;
};

wulfhilde::Renderer renderer_of(const wulfhilde::Map& map, const wulfhilde::MapAssets& assets) {
    auto renderer = wulfhilde::Renderer::create_headless(gpu_test_settings);
    require_value(renderer);
    require_value(renderer->set_map(map, assets));
    return *std::move(renderer);
}

View view(wulfhilde::Renderer& renderer, const wulfhilde::Camera& camera) {
    auto image = renderer.render_image(camera, image_extent);
    require_value(image);
    return {camera, *std::move(image)};
}

// Whether a pixel's channels that `channels` names are each well above every other.
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

// Checks what the yard's skies show of the quarters in the four diagonal directions, from an eye
// at its centre, 1.5 m up.
void check_quarters(const View& up, const View& down) {
    struct Seen {
        const View* view = nullptr;
        Vector3 point{};
        std::array<bool, 3> color{};
    };

    for (const auto& [seen_by, point, color] : {
             Seen{.view = &up, .point = {.x = 3.0F, .y = 3.0F, .z = 3.0F}, .color = red},
             Seen{.view = &up, .point = {.x = 1.0F, .y = 3.0F, .z = 3.0F}, .color = blue},
             Seen{.view = &up, .point = {.x = 3.0F, .y = 1.0F, .z = 3.0F}, .color = green},
             Seen{.view = &up, .point = {.x = 1.0F, .y = 1.0F, .z = 3.0F}, .color = yellow},
             Seen{.view = &down, .point = {.x = 3.0F, .y = 3.0F, .z = 0.0F}, .color = blue},
             Seen{.view = &down, .point = {.x = 1.0F, .y = 3.0F, .z = 0.0F}, .color = red},
             Seen{.view = &down, .point = {.x = 3.0F, .y = 1.0F, .z = 0.0F}, .color = yellow},
             Seen{.view = &down, .point = {.x = 1.0F, .y = 1.0F, .z = 0.0F}, .color = green},
         }) {
        const auto pixel = seen_by->at(point);
        CAPTURE(point, describe(pixel));
        CHECK(shows(pixel, color));
    }
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a sky shows its backdrop in the direction it is seen, with +X at the image's "
                 "centre, +Y to its left and straight up at its top",
                 "[renderer]") {
    const auto map = yard_map();
    auto renderer = renderer_of(map, yard_assets(quarters()));
    const Vector3 eye{.x = 2.0F, .y = 2.0F, .z = 1.5F};

    check_quarters(view(renderer, camera_at(eye, Angle{}, Angle::degrees(90.0F))),
                   view(renderer, camera_at(eye, Angle{}, Angle::degrees(-90.0F))));
}

TEST_CASE_METHOD(GpuTest,
                 "a sky's backdrop is fixed to its sector's coordinates, so it stays put as the "
                 "eye turns",
                 "[renderer]") {
    const auto map = yard_map();
    auto renderer = renderer_of(map, yard_assets(quarters()));
    const Vector3 eye{.x = 2.0F, .y = 2.0F, .z = 1.5F};

    check_quarters(view(renderer, camera_at(eye, Angle::degrees(120.0F), Angle::degrees(90.0F),
                                            Angle::degrees(90.0F))),
                   view(renderer, camera_at(eye, Angle::degrees(-60.0F), Angle::degrees(-90.0F),
                                            Angle::degrees(45.0F))));
}

TEST_CASE_METHOD(GpuTest,
                 "a sky's backdrop is infinitely far away, so eyes in different places see the "
                 "same of it looking the same way",
                 "[renderer]") {
    const auto map = yard_map();
    auto renderer = renderer_of(map, yard_assets(patchwork()));
    const auto yaw = Angle::degrees(30.0F);
    const auto pitch = Angle::degrees(40.0F);

    const auto near = view(renderer, camera_at({.x = 1.0F, .y = 1.0F, .z = 1.0F}, yaw, pitch));
    const auto far = view(renderer, camera_at({.x = 2.5F, .y = 3.0F, .z = 2.0F}, yaw, pitch));

    const auto seen_near = near.centre();
    const auto seen_far = far.centre();
    CAPTURE(describe(seen_near), describe(seen_far));
    for (std::size_t channel = 0; channel < 3; ++channel) {
        CHECK(std::abs(seen_near.at(channel) - seen_far.at(channel)) <= 2);
    }
}

TEST_CASE_METHOD(GpuTest, "a sky shines at its own nits, or else at its backdrop's", "[renderer]") {
    const auto camera =
        camera_at({.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle{}, Angle::degrees(90.0F));
    const auto backdrops = yard_map();
    auto backdrops_renderer = renderer_of(backdrops, yard_assets(white()));
    const auto own = yard_map(2.0F);
    auto own_renderer = renderer_of(own, yard_assets(white()));

    const auto at_backdrops = view(backdrops_renderer, camera).centre();
    const auto at_own = view(own_renderer, camera).centre();

    CAPTURE(describe(at_backdrops), describe(at_own));
    // White at 20 nits shows a little brighter than mid-grey, and at 2 nits much darker.
    CHECK(at_backdrops.at(0) > 128);
    CHECK(at_own.at(0) + 64 < at_backdrops.at(0));
    CHECK(at_backdrops.at(0) == at_backdrops.at(2));
    CHECK(at_own.at(0) == at_own.at(2));
}
