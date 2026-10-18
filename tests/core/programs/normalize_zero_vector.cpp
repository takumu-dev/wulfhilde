#include "precondition_test.hpp"
#include <wulfhilde/core/vector3.hpp>

void break_precondition() {
    [[maybe_unused]] const auto direction = wulfhilde::normalize(wulfhilde::Vector3{});
}
