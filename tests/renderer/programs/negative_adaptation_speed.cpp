#include "precondition_test.hpp"
#include <wulfhilde/renderer/renderer.hpp>

void break_precondition() {
    [[maybe_unused]] const auto renderer =
        wulfhilde::Renderer::create_headless({.exposure_adaptation_speed = -1.0F});
}
