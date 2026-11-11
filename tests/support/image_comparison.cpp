#include "image_comparison.hpp"

#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/image.hpp>

#include <lodepng.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

bool updating_references() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe): nothing sets the environment while the tests run
    const char* update = std::getenv("WULFHILDE_UPDATE_REFERENCES");
    return update != nullptr && std::string_view(update) == "1";
}

} // namespace

ImageComparison compare_images(const wulfhilde::Image& actual, const wulfhilde::Image& expected,
                               std::uint8_t channel_tolerance) {
    constexpr auto pixel_size = wulfhilde::Image::bytes_per_pixel;
    std::vector<std::uint8_t> difference(expected.pixels().size());
    const std::span<std::uint8_t> shown_pixels(difference);
    std::size_t differing_pixels = 0;
    std::uint8_t largest_difference = 0;
    for (std::size_t start = 0; start < difference.size(); start += pixel_size) {
        const auto actual_pixel = actual.pixels().subspan(start, pixel_size);
        const auto expected_pixel = expected.pixels().subspan(start, pixel_size);
        std::uint8_t largest = 0;
        for (const auto [actual_channel, expected_channel] :
             std::views::zip(actual_pixel, expected_pixel)) {
            const int apart = std::abs(int{actual_channel} - int{expected_channel});
            largest = std::max(largest, wulfhilde::narrow<std::uint8_t>(apart));
        }
        largest_difference = std::max(largest_difference, largest);
        std::array<std::uint8_t, pixel_size> shown{255, 0, 0, 255};
        if (largest > channel_tolerance) {
            ++differing_pixels;
        } else {
            std::ranges::transform(
                expected_pixel | std::views::take(3), shown.begin(),
                [](std::uint8_t channel) { return wulfhilde::narrow<std::uint8_t>(channel / 3); });
        }
        std::ranges::copy(shown, shown_pixels.subspan(start, pixel_size).begin());
    }
    return {
        .differing_pixels = differing_pixels,
        .largest_difference = largest_difference,
        .difference = wulfhilde::Image(expected.extent(), std::move(difference)),
    };
}

std::optional<wulfhilde::Image> read_png(const std::filesystem::path& path) {
    std::vector<unsigned char> pixels;
    unsigned width = 0;
    unsigned height = 0;
    if (lodepng::decode(pixels, width, height, path.string()) != 0 || width == 0 || height == 0) {
        return std::nullopt;
    }
    return wulfhilde::Image({.width = width, .height = height},
                            std::vector<std::uint8_t>(pixels.begin(), pixels.end()));
}

bool write_png(const std::filesystem::path& path, const wulfhilde::Image& image) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const auto pixels = image.pixels();
    return lodepng::encode(path.string(), pixels.data(),
                           wulfhilde::narrow<unsigned>(image.extent().width),
                           wulfhilde::narrow<unsigned>(image.extent().height)) == 0;
}

std::optional<std::string> check_reference_image(const wulfhilde::Image& actual,
                                                 std::string_view name,
                                                 const ImageReferences& references,
                                                 ImageTolerance tolerance) {
    const auto reference_path = references.references / std::format("{}.png", name);
    if (updating_references()) {
        if (!write_png(reference_path, actual)) {
            return std::format("couldn't write the reference {}", reference_path.string());
        }
        return std::nullopt;
    }

    const auto actual_path = references.failures / std::format("{}.actual.png", name);
    const auto expected = read_png(reference_path);
    if (!expected) {
        write_png(actual_path, actual);
        return std::format("no reference image could be read at {}; render it in CI's Linux image "
                           "with WULFHILDE_UPDATE_REFERENCES=1. The image drawn is at {}",
                           reference_path.string(), actual_path.string());
    }
    if (expected->extent() != actual.extent()) {
        write_png(actual_path, actual);
        return std::format("the image drawn is {}x{}, and its reference {} is {}x{}. The image "
                           "drawn is at {}",
                           actual.extent().width, actual.extent().height, reference_path.string(),
                           expected->extent().width, expected->extent().height,
                           actual_path.string());
    }

    const auto comparison = compare_images(actual, *expected, tolerance.channel);
    const auto pixel_count = actual.extent().width * actual.extent().height;
    const auto allowed = static_cast<std::size_t>(
        std::floor(tolerance.differing_share * static_cast<double>(pixel_count)));
    if (comparison.differing_pixels <= allowed) {
        return std::nullopt;
    }
    const auto difference_path = references.failures / std::format("{}.difference.png", name);
    write_png(actual_path, actual);
    write_png(difference_path, comparison.difference);
    return std::format("{} of {} pixels differ from {} by more than {} in a channel, where {} may; "
                       "the largest difference is {}. The image drawn is at {}, and the pixels "
                       "that differ are red in {}",
                       comparison.differing_pixels, pixel_count, reference_path.string(),
                       tolerance.channel, allowed, comparison.largest_difference,
                       actual_path.string(), difference_path.string());
}
