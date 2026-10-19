#include "precondition_test.hpp"
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>

void break_precondition() {
    // A map of one triangular sector, whose only sector ID is 0.
    const auto
        map =
            wulfhilde::Map::create(
                {
                    .sectors =
                        {
                            {
                                .id = "hall",
                                .floor = {.height = 0.0F, .material = "test/floor"},
                                .ceiling = {.height = 3.0F, .material = "test/ceiling"},
                                .loops =
                                    {
                                        {
                                            {
                                                .start = {.x = 0.0F, .y = 0.0F},
                                                .surface = {.material = "test/wall"},
                                            },
                                            {
                                                .start = {.x = 4.0F, .y = 0.0F},
                                                .surface = {.material = "test/wall"},
                                            },
                                            {
                                                .start = {.x = 0.0F, .y = 4.0F},
                                                .surface = {.material = "test/wall"},
                                            },
                                        },
                                    },
                            },
                        },
                    .start = {.sector = "hall", .point = {.x = 1.0F, .y = 1.0F}},
                });
    if (map) {
        [[maybe_unused]] const auto contained =
            map->contains({.sector = wulfhilde::SectorId{1}, .point = {.x = 1.0F, .y = 1.0F}});
    }
}
