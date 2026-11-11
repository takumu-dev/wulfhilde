# Test content

A content root of hand-written test maps. Each proves one area of the engine's features, with its
cases side by side, and every one of them loads without a problem. The viewer opens any of them:

```sh
wulfhilde-viewer tests/content/sectors.map.json
```

Every map here passes `schema/map.schema.json`, and every material `schema/material.schema.json`,
which the `lint` preset checks. The viewer's tests load each map and its assets through the assets
module.

This file is also the manual checklist, run on every release branch before the release is
finished: open each map in the viewer, and look at what it says needs eyes.

## The maps

### `sectors`

One concave sector, the hall: an L of two arms 4 m wide, one 12 m long along X and the other 10 m
long along Y, under a ceiling 4 m up. The player starts in the arm along X, facing down it. Its surfaces name the
debug-grid materials `materials/grid/floor`, `materials/grid/ceiling` and `materials/grid/wall`,
which are plain colours, one for each role, until they have their grid textures.

- **Cases**: a concave sector, which no convex piece needs to be cut from.
- **Needs eyes**: nothing yet. The viewer loads the map and reports that it loaded, but doesn't
  draw it.
