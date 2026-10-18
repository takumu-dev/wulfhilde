#include <wulfhilde/core/id.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>

using wulfhilde::Id;

namespace {

using AppleId = Id<struct AppleTag>;
using PearId = Id<struct PearTag>;

} // namespace

TEST_CASE("an ID keeps the index it is made with", "[core]") {
    STATIC_CHECK(AppleId{7}.index() == 7);
    STATIC_CHECK(AppleId{0}.index() == 0);
    STATIC_CHECK(AppleId{4'294'967'295U}.index() == 4'294'967'295U);
}

TEST_CASE("IDs of one kind compare and order by their indices", "[core]") {
    STATIC_CHECK(AppleId{3} == AppleId{3});
    STATIC_CHECK(AppleId{3} != AppleId{4});
    STATIC_CHECK(AppleId{3} < AppleId{4});
    STATIC_CHECK(AppleId{5} > AppleId{4});
}

TEST_CASE("an ID is only made from an index on purpose, and IDs of different kinds don't mix",
          "[core]") {
    STATIC_CHECK_FALSE(std::is_convertible_v<std::uint32_t, AppleId>);
    STATIC_CHECK_FALSE(std::is_convertible_v<AppleId, PearId>);
    STATIC_CHECK_FALSE(std::is_constructible_v<AppleId, PearId>);
    STATIC_CHECK_FALSE(std::equality_comparable_with<AppleId, PearId>);
}

TEST_CASE("an ID has no value that means no object, which an empty optional says instead",
          "[core]") {
    STATIC_CHECK_FALSE(std::is_default_constructible_v<AppleId>);
    STATIC_CHECK_FALSE(std::optional<AppleId>{}.has_value());
    STATIC_CHECK(sizeof(AppleId) == sizeof(std::uint32_t));
}
