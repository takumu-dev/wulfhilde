# Test content

A content root of hand-written test maps. Each proves one area of the engine's features, with its
cases side by side, and every one of them loads without a problem. The viewer opens any of them:

```sh
wulfhilde-viewer tests/content/sectors.map.json
```

Every map here passes `schema/map.schema.json`, every material `schema/material.schema.json` and
every backdrop `schema/backdrop.schema.json`, which the `lint` preset checks. The viewer's tests load each map and its assets through the assets
module, and render each map's cameras, comparing each image with its reference in
`tests/viewer/references/<map>/<camera>.png`. Each map has a timed benchmark walk, which
`wulfhilde-viewer <map> --benchmark <walk>` plays, apart from the goal-based walks of the viewer's
tests; the walks are a table in `apps/viewer/benchmark_walks.cpp`.

The debug-grid materials in `materials/grid/` are written by `write_debug_content.py`, which
needs Python 3 and nothing beyond its standard library, and their files are committed. There is
one for each role a surface plays: `floor`, `ceiling`, `wall`, `upper_strip`, `lower_strip`, and
`portal_side` for the solid part of a transformed portal's side. Each is a 512 px colour texture
covering 1 m, in a colour of its own, ruled every metre and every 0.25 m, with arrows along +u and
+v from the corner where they start, and the role's name. To change them, change the script, run
it, and commit what it writes. It writes the debug compass backdrops in `backdrops/` too, one in
blue and one in amber, each an equirectangular image of 1024 by 512 px at 2,000 nits: a sky of its
tint above a darker ground, with a dark line along the horizon, rings every 30° of elevation, a
meridian every 45° of turn, `+X`, `+Y`, `-X` and `-Y` at 15° and 45° above and below the horizon,
a light cap straight up and a dark cap straight down.

```sh
python3 tests/content/write_debug_content.py
```

The other materials are written by hand: `materials/lamp`, which glows, and the few CC0 materials
from ambientCG, each a `materials/<name>.material.json` beside its textures in `materials/<name>/`,
whose sources `CREDITS.md` records.

This file is also the manual checklist, run on every release branch before the release is
finished: open each map in the viewer, and look at what it says needs eyes.

## The maps

### `sectors`

A concave sector, the hall: an L of two arms 4 m wide, one 12 m long along X and the other 10 m
long along Y, under a ceiling 4 m up, in white ambient light of 250 lux. The player starts in the
arm along X, facing down it. Its surfaces are dressed in the debug-grid materials of their roles.

At the end of the arm along Y, a doorway 2 m wide opens north into the gallery, a square 4 m on a
side at the hall's heights. The gallery's whole east wall opens into the bay, another square,
whose ceiling is a metre higher, so that the bay's side of that opening has an upper strip.

The arm along X has each long side split into walls whose materials lie differently, each lined up
with its neighbour's grid, strips included. Along the south side, from x = 0: a plain wall to
x = 4.6, split about the opening into the low passage between x = 1 and 3; a wall offset by 4.6 m
along u, which carries the first wall's grid on; and from x = 8.3, a wall offset by 8.3 m, turned
90° and at half the size, whose 0.5 m lines meet the others' 0.25 m and 1 m lines. Along the north
side, from x = 12: a wall at twice the size, split about the foot of the stair between x = 9.5 and
7.5, then one turned 180° and offset by 4.5 m, whose lines still fall on whole metres, split about
the doorway into the nook between x = 6.5 and 5. Each piece is offset by its distance along the
side, so its grid and its strip's carry on from the piece before.

The steps and openings straddle the player's limits, a 0.4 m step and a body 1.8 m tall, with clear
margins. North from the arm along X, a stair of three steps, each 0.3 m high and 0.5 m deep with a
lower strip for its riser, climbs onto the ledge, 0.9 m up; and a doorway 1.5 m wide and 0.5 m deep,
under a ceiling 2 m up that leaves an upper strip on either side, leads into the nook. Through the
hall's west wall between y = 1 and 3, behind the start, is the high step, a sector 0.6 m up; and
through the south wall, to the start's right, the low passage, under a ceiling 1.6 m up.

Up the arm along Y stand two inner loops of the hall, either side of the way from the start to the
gallery's doorway. On the right, a pillar 0.6 m square from (3, 6) to (3.6, 6.6) is a solid inner
loop. On the left, the dais, a sector 1 m by 2 m from (0.4, 7) to (1.4, 9), 0.3 m up, lies inside
an inner loop whose four walls are the sides of portals into it, each with a lower strip for its
rim. The grids of the walls round each run on from wall to wall, as the hall's do.

- **Cases**: a concave sector, which no convex piece needs to be cut from; walls that run to the
  right from their starts as they are seen from inside, with height as v; floors and ceilings along
  X and Y, so the ceiling's grid reads mirrored from below; a surface's offset, scale and
  rotation; sectors beyond wall portals, seen and walked into through their openings; steps the
  player walks up and down, a step too high and a passage too low; a sector with holes in it, one
  round a pillar and one round a dais reached through an inner loop of portals.
- **Cameras**: `mapping`, from the far end of the arm along X back toward the corner where the arm
  along Y opens, showing both split sides of the arm and their strips, the floor and the ceiling;
  `stairs`, from the arm along X up the stair onto the ledge, with the doorway beside it; `holes`,
  from the foot of the arm along Y up it, to the dais on the left and the pillar on the right.
- **Benchmark walk**: `tour`, 19.5 s: down the arm along X to where `mapping` stands, back down
  it, up the arm along Y and through the doorway into the gallery, across into the bay and back,
  then back through the doorway to the start, facing as the start faces.
- **Needs eyes**: the grids stay sharp along the floor and the walls far down the arm, and blur
  there with `--anisotropy 1`; every label reads the right way round, except on the ceiling, which
  is mirrored, and on the walls turned 90° and 180°. From the hall, the gallery and the bay show
  through the doorway with nothing missing at its edges, and the player walks through both openings
  without a hitch; from the bay, the upper strip shows above the opening into the gallery. The
  player walks up the stair onto the ledge and back down without stopping, the eye easing over
  each step rather than jumping, and walks through the doorway into the nook; the high step and the
  low passage stop the player. The floor and the ceiling show no seam round the pillar or the dais,
  whose floor and ceiling meet the hall's grids; the player slides round the pillar without
  snagging, and steps up onto the dais from any side and back down.

### `overlap`

Sectors that share space, and sectors that cross seen from above but share none. The yard, 16 m by
12 m under the blue compass sky 4 m up, in white ambient light of 10,000 lux, has a hut in a notch
of its north side, 4 m on a side. The hut's door, 1.2 m wide, opens into the hall, 10 m by 12 m and
3.5 m high, lit at a tenth of the yard's light: a hall bigger than the hut, which shares the yard's
space on either side of the hut and runs on north of the yard. The player starts in the yard,
facing the door.

South of the yard, a chasm 4 m across falls 3 m to a floor. The bridge, 2 m wide, crosses it at the
yard's height to the bank beyond. Under the bridge's southern half, the passage runs east to west
from one side of the chasm to the other, its ceiling the bridge's floor, so the two cross seen
from above without sharing space. The bridge's northern half is open to the chasm on both sides,
and its southern half walled where the passage runs beneath, since one wall can open onto only one
sector. The chasm's walls below the yard, the bridge and the bank are their lower strips, and above
the passage's mouths, their upper strips.

- **Cases**: a sector that shares another's space, seen through an opening into it from either
  side, confined to the opening and to what lies beyond it, with neither showing through the
  other; a body in one, held by its own walls alone; and a bridge over a passage, drawn by depth
  alone in one view.
- **Cameras**: `bridge`, from the chasm's floor, east at the passage's mouth under the bridge's
  walled half and the pier under its open half; `hut_outside`, from the yard at the hut's door; `hut_inside`, from the
  hall's north-west, back at the door.
- **Benchmark walk**: `tour`, 21.4 s: into the hall, looking all round it, and back out across the
  yard and the bridge to the bank; back into the yard, then down into the chasm beside the bridge,
  and east under the bridge through the passage to the chasm beyond.
- **Needs eyes**: from the yard, the hall shows only through the door, and from the hall, the yard
  only through the door, with no seam or flicker at the door's edges, and none as the player walks
  through it or stands in it; in the hall, the yard's walls about the hut stop nothing; from the
  bridge's open half, the chasm's floor shows below on either side; and from the chasm, the passage
  shows through its mouth below the bridge's walled half, with the chasm beyond at its far end.

### `outdoors`

The yard, a square 8 m on a side under a blue compass sky 6 m up, opens east across its whole wall
into the field, another square under the amber compass sky, shown at 500 nits rather than the
backdrop's 2,000. Both lie in white ambient light of 10,000 lux. The player starts in the yard,
facing south over its edge, where an abyss 6 m across falls to a sky floor 8 m down that shows the
blue backdrop's lower half. A bridge 2 m wide crosses the abyss at the yard's height to the ledge
beyond, and the abyss's walls beside the bridge, the yard and the ledge are their lower strips.

- **Cases**: two neighbouring skies of different backdrops, one at nits of its own; skies fixed to
  their sectors' coordinates, which share the same axes through ordinary portals, so the meridians
  and rings carry on across the opening; and a sky floor that a body falls onto and stands on.
- **Cameras**: `skies`, from the yard's west side, east and up across the opening into the field;
  `abyss`, from the yard's edge, down into the abyss beside the bridge.
- **Benchmark walk**: `tour`, 11.25 s: south across the bridge to the ledge, back into the yard,
  then east into the field, turning to look back.
- **Needs eyes**: `+X` stands at the meridian straight east of the yard, `+Y` to its left and the
  light cap straight up; the meridians and rings run on unbroken from the yard's sky into the
  field's, which is dimmer; no line shows down the backdrop's seam behind `-X`; looking down from
  the bridge, the abyss shows the ground's labels and the dark cap; and a player who steps off the
  bridge falls and stands on the abyss's floor.

### `lights`

The hall, 8 m by 6 m under a ceiling 3.5 m up, lies in the map's white ambient light of 300 lux.
Its floor is glossy limestone tiles, with relief in their normal texture, its south wall is
corrugated steel and its other walls are plaster, the CC0 materials credited in `CREDITS.md`. Two
lamps, each a wall 1 m wide of the same lamp material, glow on its north wall: the right one at the
material's own 400 nits of 3,000 K, and the left one at the 80 nits its surface sets. A doorway
1.5 m wide in the east wall opens into the annex, 4 m square under a ceiling 3 m up, which is lit by
ambient light of its own, 75 lux of a warm white, and whose far wall is corrugated steel too. The
player starts in the hall, facing east toward the doorway.

- **Cases**: a material's emission at its own nits, and at a surface's; the map's ambient light and
  a sector's own, stepping down at the portal between them; glTF's metallic-roughness materials
  with every property, and specular anti-aliasing on a glossy floor with relief.
- **Cameras**: `emission`, across the hall at the two lamps; `ambient`, from the hall through the
  doorway into the annex.
- **Benchmark walk**: `tour`, 6.75 s: east through the doorway into the annex, back into the hall,
  and round to stand facing the lamps.
- **Needs eyes**: the steel reads as metal and the tiles and plaster as stone, and the tiles' joints
  stand in relief; the floor's highlights don't sparkle or crawl as the player walks; the lamps
  glow without lighting the floor or the walls beside them, the right one five times as bright as
  the left; the annex is darker and warmer than the hall, with the step in brightness exactly at
  the opening, and exposure eases between them as the player walks through.
