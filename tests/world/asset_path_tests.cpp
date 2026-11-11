#include <wulfhilde/world/asset_path.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>

using wulfhilde::is_asset_path;

TEST_CASE("an asset path is segments of lowercase letters, digits, underscores and hyphens, "
          "separated by slashes",
          "[world]") {
    for (const std::string_view path :
         {"floor", "test/floor", "stone_2/rough-floor", "a/b/c/d/e"}) {
        CAPTURE(path);
        CHECK(is_asset_path(path));
    }
}

TEST_CASE("an asset path has no extension, no capitals, no empty segment and no way out of its "
          "content root",
          "[world]") {
    for (const std::string_view path : {
             "",
             "floor.png",
             "Floor",
             "test//floor",
             "/test/floor",
             "test/floor/",
             "../floor",
             "test/./floor",
             "test\\floor",
             "test floor",
             "fl\xc3\xb6r",
         }) {
        CAPTURE(path);
        CHECK(!is_asset_path(path));
    }
}
