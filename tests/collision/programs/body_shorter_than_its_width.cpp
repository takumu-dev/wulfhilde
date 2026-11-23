#include "precondition_test.hpp"
#include <wulfhilde/collision/collision.hpp>
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
        // A capsule's height includes both its caps, so it can't be less than its width.
        [[maybe_unused]] const auto settled =
            wulfhilde::settle(*map, {.radius = 0.5F, .height = 0.8F},
                              {
                                  .centre =
                                      {
                                          .sector = wulfhilde::SectorId{0},
                                          .point = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
                                      },
                              });
    }
}
