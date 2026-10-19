// The viewer's acceptance suite: the test maps in tests/content, loaded through the assets module
// as the viewer loads them.
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

TEST_CASE("every test map loads without a problem", "[viewer]") {
    const std::filesystem::path content = WULFHILDE_TEST_CONTENT;
    const auto root = wulfhilde::ContentRoot::open(content);
    REQUIRE(root.has_value());

    constexpr std::string_view extension = wulfhilde::map_extension;
    std::size_t maps = 0;
    for (const auto& entry : std::filesystem::directory_iterator(content)) {
        const auto name = entry.path().filename().string();
        if (!name.ends_with(extension)) {
            continue;
        }
        ++maps;
        CAPTURE(name);
        const auto map = wulfhilde::load_map(*root, name.substr(0, name.size() - extension.size()));
        if (!map) {
            FAIL_CHECK(std::format("{}", map.error()));
        }
    }
    CHECK(maps > 0);
}

TEST_CASE("the sectors map is one concave sector", "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "sectors");

    REQUIRE(map.has_value());
    REQUIRE(map->sectors().size() == 1);
    CHECK(map->sectors().front().loops.front().size() == 6);
    CHECK(map->sector(map->start().location.sector).id == "hall");
    CHECK(map->contains(map->start().location));
}
