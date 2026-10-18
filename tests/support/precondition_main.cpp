// The main program of a precondition test. A precondition that fires calls the assertion handler,
// which this replaces with one that fails the process. CTest expects the test to fail; if the
// precondition doesn't fire, the program succeeds and the test fails.
//
// The default handler would abort, but CTest counts an abort as a crash even in a test expected to
// fail.
#include "precondition_test.hpp"
#include <wulfhilde/core/assertion.hpp>

#include <cstdlib>

namespace {

[[noreturn]] void exit_failure(const wulfhilde::AssertionFailure& /*failure*/) {
    std::_Exit(EXIT_FAILURE);
}

} // namespace

int main() {
    wulfhilde::set_assertion_handler(exit_failure);
    break_precondition();
    return EXIT_SUCCESS;
}
