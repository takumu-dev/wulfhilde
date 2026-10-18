#include "precondition_test.hpp"
#include <wulfhilde/core/narrow.hpp>

#include <cstdint>

void break_precondition() {
    [[maybe_unused]] const auto narrowed = wulfhilde::narrow<std::uint32_t>(-1);
}
