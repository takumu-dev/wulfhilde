// The platform's errors, as SDL reports its failures.
#pragma once

#include <wulfhilde/platform/platform_error.hpp>

#include <SDL3/SDL_error.h>

#include <format>
#include <string_view>

namespace wulfhilde::detail {

/// The error of a failure that SDL has just reported: what was being done, then SDL's reason.
///
/// \param code What kind of failure it is.
/// \param what What failed, such as "the window couldn't open".
[[nodiscard]] inline PlatformError sdl_error(PlatformErrorCode code, std::string_view what) {
    return {.code = code, .message = std::format("{}: {}", what, SDL_GetError())};
}

} // namespace wulfhilde::detail
