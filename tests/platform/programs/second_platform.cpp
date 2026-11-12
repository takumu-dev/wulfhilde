#include "precondition_test.hpp"
#include <wulfhilde/platform/platform.hpp>

void break_precondition() {
    const auto first = wulfhilde::Platform::create();
    // A first platform that can't come up proves nothing, so the program succeeds and the test
    // fails.
    if (!first) {
        return;
    }
    [[maybe_unused]] const auto second = wulfhilde::Platform::create();
}
