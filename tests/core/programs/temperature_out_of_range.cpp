#include "precondition_test.hpp"
#include <wulfhilde/core/color.hpp>

void break_precondition() {
    [[maybe_unused]] const auto color = wulfhilde::Color::from_temperature(500.0F);
}
