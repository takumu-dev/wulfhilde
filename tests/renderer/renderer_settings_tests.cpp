#include <wulfhilde/renderer/renderer_settings.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a renderer filters textures 16 times anisotropically by default", "[renderer]") {
    STATIC_CHECK(wulfhilde::RendererSettings{}.anisotropy == 16);
}

TEST_CASE("a renderer exposes automatically by default, adapting over a second or two",
          "[renderer]") {
    STATIC_CHECK(!wulfhilde::RendererSettings{}.pinned_exposure.has_value());
    STATIC_CHECK(wulfhilde::RendererSettings{}.exposure_adaptation_speed == 2.0F);
}

TEST_CASE("a renderer nests views 4 deep and draws 16 views a frame by default", "[renderer]") {
    STATIC_CHECK(wulfhilde::RendererSettings{}.nested_view_limit == 4);
    STATIC_CHECK(wulfhilde::RendererSettings{}.view_limit == 16);
}
