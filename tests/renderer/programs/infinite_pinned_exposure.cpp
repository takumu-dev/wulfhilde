#include "precondition_test.hpp"
#include <wulfhilde/renderer/renderer.hpp>

#include <limits>

void break_precondition() {
    [[maybe_unused]] const auto renderer = wulfhilde::Renderer::create_headless(
        {.pinned_exposure = std::numeric_limits<float>::infinity()});
}
