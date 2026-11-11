// Vulkan as the renderer reaches it: through volk, which loads every function at run time, so that
// nothing links the Vulkan loader. Only the renderer's own sources include this.
#pragma once

#include <wulfhilde/renderer/device_error.hpp>

#include <volk.h>

#include <algorithm>
#include <optional>
#include <span>
#include <string_view>

namespace wulfhilde::detail {

/// Joins some of Vulkan's flag bits, which are enumerators, signed, into its unsigned flags.
template <typename... Bits> [[nodiscard]] constexpr VkFlags flags_of(Bits... bits) noexcept {
    return (static_cast<VkFlags>(bits) | ...);
}

/// The text of a name in one of Vulkan's structures, which ends at its first null character.
[[nodiscard]] inline std::string_view name_of(std::span<const char> name) noexcept {
    return {name.begin(), std::ranges::find(name, '\0')};
}

/// Returns nothing for a successful result, and for a failed one the error that names what was
/// being done and the result.
[[nodiscard]] std::optional<DeviceError> failure(VkResult result, std::string_view what);

} // namespace wulfhilde::detail
