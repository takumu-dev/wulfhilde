#include <wulfhilde/renderer/device_error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("a device error formats as its message", "[renderer]") {
    const wulfhilde::DeviceError error{
        .code = wulfhilde::DeviceErrorCode::no_suitable_device,
        .message = "no Vulkan device was found",
    };

    CHECK(std::format("{}", error) == "no Vulkan device was found");
    CHECK(std::format("[{:>28}]", error) == "[  no Vulkan device was found]");
}
