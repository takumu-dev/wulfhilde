#include <wulfhilde/renderer/image.hpp>

#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/renderer/extent.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace wulfhilde {

Image::Image(Extent extent, std::vector<std::uint8_t> pixels)
    : extent_(extent), pixels_(std::move(pixels)) {
    WULFHILDE_ASSERT(extent_.width > 0 && extent_.height > 0, "an image has at least one pixel");
    WULFHILDE_ASSERT(pixels_.size() / bytes_per_pixel / extent_.width == extent_.height &&
                         pixels_.size() % (bytes_per_pixel * extent_.width) == 0,
                     "an image holds four bytes for each of its pixels");
}

} // namespace wulfhilde
