/// \file
/// The platform: the system's windows, keyboard and mouse, through SDL.

#pragma once

#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/key.hpp>
#include <wulfhilde/platform/platform_error.hpp>

#include <expected>
#include <vector>

namespace wulfhilde {

/// The system's windows, keyboard and mouse, open for as long as this lives.
///
/// Only one platform exists at a time, and like every module it is used from one thread: the one
/// that created it, which on some systems must be the program's main thread. Windows are made
/// with it and must not outlive it.
class Platform {
  public:
    /// Starts the system's video, keyboard and mouse.
    ///
    /// Only one platform exists at a time, so this must not be called while another lives.
    ///
    /// \return The platform, or why video couldn't start.
    [[nodiscard]] static std::expected<Platform, PlatformError> create();

    /// Takes over another platform, leaving it empty, fit only to be destroyed or assigned to.
    ///
    /// \param other The platform to take over.
    Platform(Platform&& other) noexcept;

    /// Takes over another platform, shutting this one down first.
    ///
    /// \param other The platform to take over.
    /// \return This platform.
    Platform& operator=(Platform&& other) noexcept;

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    /// Shuts the system's video, keyboard and mouse down, once every window is gone.
    ~Platform();

    /// Takes every event that has happened since the last call, oldest first.
    ///
    /// The keys `is_key_down` reports change only here, as the events say.
    ///
    /// \return The events, which are none when nothing has happened.
    [[nodiscard]] std::vector<Event> poll_events();

    /// Answers whether a key is held down, as of the events taken last.
    ///
    /// \param key The key.
    /// \return Whether it is down.
    [[nodiscard]] bool is_key_down(Key key) const noexcept;

  private:
    /// Makes the platform that owns the system's video.
    Platform() noexcept = default;

    /// Whether this platform owns the system's video, which a platform moved from no longer does.
    bool active_ = true;
};

} // namespace wulfhilde
