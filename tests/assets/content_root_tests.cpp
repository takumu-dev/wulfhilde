#include <wulfhilde/assets/content_root.hpp>

#include "test_files.hpp"
#include <wulfhilde/assets/asset_error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <system_error>

using wulfhilde::AssetErrorCode;
using wulfhilde::ContentRoot;

TEST_CASE("a content root opens on a directory, which it keeps as an absolute path", "[assets]") {
    const auto relative = std::filesystem::relative(test_files() / "valid");
    REQUIRE(relative.is_relative());

    const auto root = ContentRoot::open(relative);

    REQUIRE(root.has_value());
    CHECK(root->directory().is_absolute());
    CHECK(std::filesystem::equivalent(root->directory(), test_files() / "valid"));
}

TEST_CASE("a content root can't open where nothing is", "[assets]") {
    const auto missing = scratch_directory("content_root_missing") / "content";

    const auto root = ContentRoot::open(missing);

    REQUIRE(!root.has_value());
    CHECK(root.error().code == AssetErrorCode::missing_content_root);
    CHECK(std::format("{}", root.error()) ==
          std::format(R"(no directory is at "{}")", missing.string()));
}

TEST_CASE("a content root can't open on a file", "[assets]") {
    const auto file = test_files() / "valid" / "everything.map.json";

    const auto root = ContentRoot::open(file);

    REQUIRE(!root.has_value());
    CHECK(root.error().code == AssetErrorCode::not_a_directory);
    CHECK(std::format("{}", root.error()) ==
          std::format(R"("{}" isn't a directory)", file.string()));
}

TEST_CASE("a content root opens through a symbolic link to a directory", "[assets]") {
    const auto link = scratch_directory("content_root_link") / "content";
    std::error_code error;
    std::filesystem::create_directory_symlink(test_files() / "valid", link, error);
    if (error) {
        SKIP("this system can't make a symbolic link: " << error.message());
    }

    const auto root = ContentRoot::open(link);

    REQUIRE(root.has_value());
    CHECK(std::filesystem::exists(root->directory() / "everything.map.json"));
}
