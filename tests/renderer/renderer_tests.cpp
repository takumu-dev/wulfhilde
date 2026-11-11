#include <wulfhilde/renderer/renderer.hpp>

#include "gpu_test.hpp"
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/testing/test_device.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>

TEST_CASE_METHOD(GpuTest, "a headless renderer comes up under validation", "[renderer]") {
    const auto renderer = wulfhilde::Renderer::create_headless(gpu_test_settings);

    require_value(renderer);
}

TEST_CASE_METHOD(GpuTest, "the validation layers' messages reach the log sink as errors",
                 "[renderer]") {
    RecordingSink sink;
    log_to(sink);
    {
        auto device = wulfhilde::detail::TestDevice::create(gpu_test_settings);
        require_value(device);
        device->break_valid_usage();
    }

    const auto messages = sink.messages();
    CHECK(std::ranges::any_of(messages, [](const auto& message) {
        return message.first == wulfhilde::LogSeverity::error &&
               message.second.contains("VUID-VkSamplerCreateInfo-anisotropyEnable-01071");
    }));
}

// The one gpu test without validation, since it tests the switch that turns validation off.
TEST_CASE_METHOD(GpuTest, "without validation, misuse of Vulkan goes unreported", "[renderer]") {
    RecordingSink sink;
    log_to(sink);
    {
        auto device = wulfhilde::detail::TestDevice::create({.validation = false});
        require_value(device);
        device->break_valid_usage();
    }

    CHECK(sink.messages().empty());
}

TEST_CASE_METHOD(GpuTest, "a device of Vulkan 1.2 is refused, naming what it lacks", "[renderer]") {
    const auto device = wulfhilde::detail::TestDevice::create(gpu_test_settings, 2);

    REQUIRE(!device.has_value());
    CHECK(device.error().code == wulfhilde::DeviceErrorCode::no_suitable_device);
    CHECK_THAT(
        device.error().message,
        Catch::Matchers::StartsWith("no Vulkan device has every feature the renderer needs: ") &&
            Catch::Matchers::EndsWith(" lacks Vulkan 1.3"));
}
