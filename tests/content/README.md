# Test content

A content root of hand-written test maps. Each proves one area of the engine's features, with its
cases side by side, and every one of them loads without a problem. The viewer opens any of them:

```sh
wulfhilde-viewer tests/content/sectors.map.json
```

Every map here passes `schema/map.schema.json`, and every material `schema/material.schema.json`,
which the `lint` preset checks. The viewer's tests load each map and its assets through the assets
module, and render each map's cameras, comparing each image with its reference in
`tests/viewer/references/<map>/<camera>.png`.

The debug-grid materials in `materials/grid/` are written by `write_debug_content.py`, which
needs Python 3 and nothing beyond its standard library, and their files are committed. There is
one for each role a surface plays: `floor`, `ceiling`, `wall`, `upper_strip`, `lower_strip`, and
`portal_side` for the solid part of a transformed portal's side. Each is a 512 px colour texture
covering 1 m, in a colour of its own, ruled every metre and every 0.25 m, with arrows along +u and
+v from the corner where they start, and the role's name. To change them, change the script, run
it, and commit what it writes:

```sh
python3 tests/content/write_debug_content.py
```

This file is also the manual checklist, run on every release branch before the release is
finished: open each map in the viewer, and look at what it says needs eyes.

## The maps

### `sectors`

One concave sector, the hall: an L of two arms 4 m wide, one 12 m long along X and the other 10 m
long along Y, under a ceiling 4 m up, in white ambient light of 250 lux. The player starts in the
arm along X, facing down it. Its surfaces are dressed in the debug-grid materials of their roles.

The arm along X has each long side split into walls whose materials lie differently, each lined up
with its neighbour's grid. Along the south side, from x = 0: a plain wall to x = 4.6; a wall offset
by 4.6 m along u, which carries the first wall's grid on; and from x = 8.3, a wall offset by 8.3 m,
turned 90° and at half the size, whose 0.5 m lines meet the others' 0.25 m and 1 m lines. Along the
north side, from x = 12: a wall at twice the size, then from x = 7.5 one turned 180° and offset by
4.5 m, whose lines still fall on whole metres.

- **Cases**: a concave sector, which no convex piece needs to be cut from; walls that run to the
  right from their starts as they are seen from inside, with height as v; floors and ceilings along
  X and Y, so the ceiling's grid reads mirrored from below; and a surface's offset, scale and
  rotation.
- **Cameras**: `mapping`, from the far end of the arm along X back toward the corner where the arm
  along Y opens, showing both split sides of the arm, the floor and the ceiling.
- **Needs eyes**: the grids stay sharp along the floor and the walls far down the arm, and blur
  there with `--anisotropy 1`; every label reads the right way round, except on the ceiling, which
  is mirrored, and on the walls turned 90° and 180°.
