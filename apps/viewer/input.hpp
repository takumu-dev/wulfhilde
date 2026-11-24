#pragma once

#include "controller.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/key.hpp>

#include <optional>
#include <span>
#include <vector>

namespace viewer {

/// What the player asks of the viewer in one frame.
struct FrameInput {
    /// How far to turn the eye left this frame.
    wulfhilde::Angle yaw{};

    /// How far to tilt the eye up this frame.
    wulfhilde::Angle pitch{};

    /// What to ask of the body, each tick due this frame.
    Actions actions{};

    /// Whether to return to the start position.
    bool return_to_start = false;

    /// Whether to print the eye as a camera that `--camera` takes.
    bool print_eye = false;

    /// Whether the window takes the mouse for itself to look with, or lets it go, when that
    /// changes this frame.
    std::optional<bool> capture_mouse{};
};

/// Follows what the player does on the keyboard and with the mouse to tell what they ask of the
/// viewer.
///
/// The mouse looks while the window has it, which it does from the start. Esc lets it go, and a
/// click in the window takes it back.
class Input {
  public:
    /// Makes the input of a player whose mouse turns the eye by an angle for each count it moves.
    ///
    /// \param sensitivity How far the eye turns for each count.
    explicit Input(wulfhilde::Angle sensitivity) noexcept : sensitivity_(sensitivity) {}

    /// Takes in the events since the last ones, oldest first.
    ///
    /// \param events The events.
    /// \return What the player asks of the viewer this frame.
    [[nodiscard]] FrameInput follow(std::span<const wulfhilde::Event> events);

  private:
    /// Returns whether a key is held down.
    [[nodiscard]] bool held(wulfhilde::Key key) const;

    /// How far the eye turns for each count the mouse moves.
    wulfhilde::Angle sensitivity_;

    /// Whether the window has the mouse to look with.
    bool captured_ = true;

    /// The keys held down, of those the viewer uses.
    std::vector<wulfhilde::Key> held_;
};

} // namespace viewer
