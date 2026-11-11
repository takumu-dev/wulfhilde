#include <wulfhilde/world/asset_path.hpp>

#include <algorithm>
#include <ranges>
#include <string_view>

namespace wulfhilde {

bool is_asset_path(std::string_view path) noexcept {
    // Splitting an empty string gives no segments at all, rather than one empty segment.
    if (path.empty()) {
        return false;
    }
    for (const auto segment : std::views::split(path, '/')) {
        if (segment.empty() || !std::ranges::all_of(segment, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            })) {
            return false;
        }
    }
    return true;
}

} // namespace wulfhilde
