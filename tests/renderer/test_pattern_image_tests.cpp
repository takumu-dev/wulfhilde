#include "gpu_test.hpp"
#include "image_comparison.hpp"
#include "test_pattern.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

#include "test_pattern_spirv.h"

ImageReferences references() {
    return {
        .references = WULFHILDE_RENDERER_REFERENCES,
        .failures = WULFHILDE_IMAGE_FAILURES,
    };
}

} // namespace

TEST_CASE_METHOD(GpuTest, "the test pattern matches its reference", "[renderer]") {
    const auto image = test_pattern::draw(test_pattern_spirv, {.width = 160, .height = 90});

    const auto failure = check_reference_image(image, "test_pattern", references());

    INFO(failure.value_or(""));
    CHECK(!failure);
}
