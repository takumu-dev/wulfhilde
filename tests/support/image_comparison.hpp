// The image tests' harness: an image the renderer drew, compared with a reference image committed
// beside the tests.
//
// References are rendered in CI's pinned Linux image, where lavapipe draws the same on every
// machine, and only the GCC release job compares with them. To change them, run the image tests in
// that image with WULFHILDE_UPDATE_REFERENCES=1, which writes each image the tests draw over its
// reference and passes; then look at the new images and commit them with the change.
#pragma once

#include <wulfhilde/renderer/image.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// How far an image may stray from its reference and still match it.
struct ImageTolerance {
    // The most that any one channel of a pixel may differ by, in 8-bit steps.
    std::uint8_t channel = 2;

    // The share of the pixels, from 0 to 1, that may differ by more than that.
    double differing_share = 0.001;
};

// How an image compares with its reference.
struct ImageComparison {
    // The number of pixels where some channel differs by more than the tolerance allows.
    std::size_t differing_pixels = 0;

    // The most that any channel of any pixel differs by.
    std::uint8_t largest_difference = 0;

    // The image, with each differing pixel red over the reference dimmed to a third.
    wulfhilde::Image difference;
};

// Compares two images of the same size, pixel by pixel.
ImageComparison compare_images(const wulfhilde::Image& actual, const wulfhilde::Image& expected,
                               std::uint8_t channel_tolerance);

// Reads a PNG file as 8-bit sRGB pixels with alpha, or nothing if it can't be read.
std::optional<wulfhilde::Image> read_png(const std::filesystem::path& path);

// Writes an image as a PNG file, creating its directory, and says whether that worked.
bool write_png(const std::filesystem::path& path, const wulfhilde::Image& image);

// Where an image test's references are, and where a failing one leaves what it drew.
struct ImageReferences {
    // The directory of the references in the source tree, each <name>.png.
    std::filesystem::path references;

    // The directory in the build tree where a failing test writes <name>.actual.png and
    // <name>.difference.png, which CI keeps.
    std::filesystem::path failures;
};

// Compares an image with the reference named `name`, and returns nothing if it matches or why it
// doesn't. With WULFHILDE_UPDATE_REFERENCES=1 it writes the image as the reference instead.
std::optional<std::string> check_reference_image(const wulfhilde::Image& actual,
                                                 std::string_view name,
                                                 const ImageReferences& references,
                                                 ImageTolerance tolerance = {});
