#include <wulfhilde/renderer/renderer_settings.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a renderer filters textures 16 times anisotropically by default", "[renderer]") {
    STATIC_CHECK(wulfhilde::RendererSettings{}.anisotropy == 16);
}
