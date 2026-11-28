# Credits

The test content is the engine's own, under its licence, except the assets below, which come from
elsewhere under CC0 1.0. Nothing under `tests/content/` is installed or shipped, so these are
recorded here rather than in `THIRD_PARTY_NOTICES.md`. ambientCG credits no individual author.

| Asset path | Source | Author | Licence | Changes |
|---|---|---|---|---|
| `materials/tiles/color`, `materials/tiles/normal`, `materials/tiles/roughness` | ambientCG, [Tiles139](https://ambientcg.com/view?id=Tiles139), 1K-PNG | ambientCG | CC0-1.0 | Resized from 1024 to 512 px; alpha dropped; normal narrowed from 16 to 8 bits per channel; NormalGL chosen; displacement and NormalDX dropped |
| `materials/corrugated_steel/color`, `materials/corrugated_steel/normal`, `materials/corrugated_steel/roughness`, `materials/corrugated_steel/metallic`, `materials/corrugated_steel/occlusion` | ambientCG, [CorrugatedSteel009](https://ambientcg.com/view?id=CorrugatedSteel009), 1K-PNG | ambientCG | CC0-1.0 | Resized from 1024 × 512 to 512 × 256 px; alpha dropped; normal narrowed from 16 to 8 bits per channel; NormalGL chosen; displacement and NormalDX dropped |
| `materials/plaster/color`, `materials/plaster/normal`, `materials/plaster/roughness` | ambientCG, [Plaster001](https://ambientcg.com/view?id=Plaster001), 1K-PNG | ambientCG | CC0-1.0 | Resized from 1024 to 512 px; normal narrowed from 16 to 8 bits per channel; NormalGL chosen; displacement and NormalDX dropped |

The materials' JSON files beside them are written by hand. ambientCG gives Tiles139 as about 2 m
square, which `materials/tiles` repeats at; it gives no size for the other two, so
`materials/corrugated_steel` repeats every 2 m by 1 m and `materials/plaster` every 2 m.
