#include <wulfhilde/core/narrow.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

TEST_CASE("narrowing keeps a value that fits", "[core]") {
    CHECK(wulfhilde::narrow<std::uint8_t>(std::size_t{255}) == 255);
    CHECK(wulfhilde::narrow<std::int8_t>(-128) == -128);
    CHECK(wulfhilde::narrow<std::uint32_t>(std::int64_t{4'294'967'295}) == 4'294'967'295U);
    CHECK(wulfhilde::narrow<std::size_t>(0) == 0U);
}

TEST_CASE("narrowing works in a constant expression", "[core]") {
    STATIC_CHECK(wulfhilde::narrow<std::uint16_t>(65'535) == 65'535);
}
