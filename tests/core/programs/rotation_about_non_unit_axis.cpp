#include "precondition_test.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>

void break_precondition() {
    [[maybe_unused]] const auto rotation =
        wulfhilde::Rotation::about_axis({.x = 1.0F, .y = 1.0F}, wulfhilde::Angle::degrees(90.0F));
}
