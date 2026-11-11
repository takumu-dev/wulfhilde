// The files the assets tests read, and a place for the files they write.
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

// The directory of the assets tests' own files: tests/assets in the source tree.
inline std::filesystem::path test_files() { return WULFHILDE_ASSETS_TEST_FILES; }

// The whole text of a file.
inline std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Writes a file, replacing any that is there.
inline void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.is_open());
    file << text;
    REQUIRE(file.good());
}

// An empty directory of the build tree for one test case to write files into, named so that no
// other test case shares it, since CTest may run them at once.
inline std::filesystem::path scratch_directory(std::string_view name) {
    const auto directory = std::filesystem::path(WULFHILDE_ASSETS_SCRATCH) / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}
