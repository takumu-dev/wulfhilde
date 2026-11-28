#!/usr/bin/env python3
"""Writes the test content's debug-grid materials, one for each role a surface plays, and its
debug compass backdrops, one in each of two tints.

Each material's colour texture is a 512 px PNG covering 1 m, ruled every metre and every 0.25 m,
with arrows along +u and +v from the corner where u and v start, and the role's name, so that how a
surface lies, which way, how large and where it starts can be told at a glance. Every role has a
colour of its own.

Each backdrop's image is an equirectangular PNG of 1024 by 512 px: a sky of its tint above a
darker ground, with a line along the horizon, rings every 30 degrees of elevation, a meridian
every 45 degrees of turn, labels at +X, +Y, -X and -Y above and below the horizon, and marks
straight up and straight down, so that which way a sky faces, and how it tilts, can be told at a
glance.

Run it with Python 3 from anywhere, and commit what it writes. It uses the standard library alone,
and writes the same files every time:

    python3 tests/content/write_debug_content.py
"""

import json
import struct
import zlib
from pathlib import Path

CONTENT = Path(__file__).resolve().parent
GRID = CONTENT / "materials" / "grid"
BACKDROPS = CONTENT / "backdrops"

SIZE = 512
METRES = 1
SUB_LINES = 4

# Each role's asset name, the name its texture shows, and its background colour in sRGB.
ROLES = [
    ("floor", "FLOOR", (176, 124, 72)),
    ("ceiling", "CEILING", (96, 128, 176)),
    ("wall", "WALL", (112, 156, 96)),
    ("upper_strip", "UPPER STRIP", (152, 104, 168)),
    ("lower_strip", "LOWER STRIP", (72, 148, 148)),
    ("portal_side", "PORTAL SIDE", (180, 84, 84)),
]

LINE = (32, 32, 32)
INK = (244, 244, 236)

BACKDROP_WIDTH = 1024
BACKDROP_HEIGHT = 512

# Each backdrop's asset name, its sky's colour in sRGB, and the luminance of its white in nits.
TINTS = [
    ("compass_blue", (112, 160, 216), 2000),
    ("compass_amber", (224, 160, 88), 2000),
]

# A font of 5 by 7 cells, each glyph its rows from the top, with only the characters the textures
# need.
FONT = {
    " ": ["00000"] * 7,
    "+": ["00000", "00100", "00100", "11111", "00100", "00100", "00000"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
}


class Canvas:
    """An RGB image addressed from its bottom-left corner, x to the right and y up, as u and v
    run."""

    def __init__(self, width, height, color):
        self.width = width
        self.height = height
        self.pixels = [bytearray(color) * width for _ in range(height)]

    def fill(self, left, bottom, width, height, color):
        """Fills a rectangle, wrapping around the edges, since a texture repeats and a backdrop
        goes around."""
        for y in range(bottom, bottom + height):
            row = self.pixels[self.height - 1 - (y % self.height)]
            for x in range(left, left + width):
                start = 3 * (x % self.width)
                row[start : start + 3] = bytes(color)

    def text(self, left, bottom, scale, words, color):
        """Writes words in the font, each cell of a glyph a square of `scale` pixels, with a cell
        between glyphs."""
        for index, character in enumerate(words):
            glyph = FONT[character]
            glyph_left = left + index * 6 * scale
            for row, cells in enumerate(glyph):
                for column, cell in enumerate(cells):
                    if cell == "1":
                        self.fill(
                            glyph_left + column * scale,
                            bottom + (6 - row) * scale,
                            scale,
                            scale,
                            color,
                        )

    def png(self):
        """Encodes the image as an 8-bit RGB PNG, with no metadata."""

        def chunk(kind, data):
            body = kind + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

        rows = b"".join(b"\x00" + bytes(row) for row in self.pixels)
        header = struct.pack(">IIBBBBB", self.width, self.height, 8, 2, 0, 0, 0)
        return (
            b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(rows, 9))
            + chunk(b"IEND", b"")
        )


def text_width(words, scale):
    return (6 * len(words) - 1) * scale


def darker(color, factor):
    return tuple(round(channel * factor) for channel in color)


def grid_texture(name, background):
    canvas = Canvas(SIZE, SIZE, background)
    step = SIZE // SUB_LINES

    # The sub-lines every 0.25 m, then the line every metre, which straddles the texture's edges,
    # so that each repeat shows half of it on each side.
    sub_line = darker(background, 0.7)
    for line in range(1, SUB_LINES):
        canvas.fill(line * step - 1, 0, 2, SIZE, sub_line)
        canvas.fill(0, line * step - 1, SIZE, 2, sub_line)
    canvas.fill(-3, 0, 6, SIZE, LINE)
    canvas.fill(0, -3, SIZE, 6, LINE)

    # Arrows along +u and +v from near the corner where both start.
    corner = 24
    length = 200
    shaft = 6
    head = 18
    canvas.fill(corner, corner, length, shaft, INK)
    middle = corner + shaft // 2
    for back in range(head):
        half = head - back
        canvas.fill(corner + length + back, middle - half, 1, 2 * half, INK)
    canvas.fill(corner, corner, shaft, length, INK)
    for back in range(head):
        half = head - back
        canvas.fill(middle - half, corner + length + back, 2 * half, 1, INK)
    canvas.text(corner + length - 40, corner + 3 * shaft, 4, "+U", INK)
    canvas.text(corner + 3 * shaft, corner + length - 40, 4, "+V", INK)

    # The role's name, across the middle.
    scale = 6
    canvas.text((SIZE - text_width(name, scale)) // 2, (SIZE - 7 * scale) // 2, scale, name, INK)
    return canvas.png()


def backdrop_row(elevation):
    """Returns the row of a backdrop's image, counted up from its bottom, at an elevation in
    degrees."""
    return round((elevation + 90) / 180 * BACKDROP_HEIGHT)


def backdrop_column(turn):
    """Returns the column of a backdrop's image at a turn anticlockwise from +X, in degrees: its
    centre faces +X, and moving left turns toward +Y."""
    return round((0.5 - turn / 360) * BACKDROP_WIDTH)


def backdrop_image(tint):
    canvas = Canvas(BACKDROP_WIDTH, BACKDROP_HEIGHT, tint)
    ground = darker(tint, 0.45)
    horizon = backdrop_row(0)
    canvas.fill(0, 0, BACKDROP_WIDTH, horizon, ground)

    # A meridian every 45 degrees of turn, short of the marks at either end, then the rings and the
    # horizon over them.
    meridian = darker(tint, 0.8)
    for turn in range(0, 360, 45):
        bottom = backdrop_row(-75)
        canvas.fill(backdrop_column(turn) - 1, bottom, 2, backdrop_row(75) - bottom, meridian)
    for elevation in (-60, -30, 30, 60):
        canvas.fill(0, backdrop_row(elevation) - 1, BACKDROP_WIDTH, 2, darker(tint, 0.65))
    canvas.fill(0, horizon - 3, BACKDROP_WIDTH, 6, LINE)

    # A label at each axis, 15 and 45 degrees above the horizon and below it, each on its own
    # hemisphere's colour, which hides the meridian behind it.
    scale = 4
    for turn, name in ((0, "+X"), (90, "+Y"), (180, "-X"), (270, "-Y")):
        width = text_width(name, scale)
        left = backdrop_column(turn) - width // 2
        for elevation in (45, 15, -15, -45):
            bottom = backdrop_row(elevation) - (7 * scale) // 2
            margin = 2 * scale
            canvas.fill(
                left - margin,
                bottom - margin,
                width + 2 * margin,
                7 * scale + 2 * margin,
                tint if elevation > 0 else ground,
            )
            canvas.text(left, bottom, scale, name, INK)

    # Straight up, a light cap ringed in dark; straight down, a dark cap ringed in light.
    canvas.fill(0, backdrop_row(80), BACKDROP_WIDTH, BACKDROP_HEIGHT - backdrop_row(80), INK)
    canvas.fill(0, backdrop_row(78) - 2, BACKDROP_WIDTH, 4, LINE)
    canvas.fill(0, 0, BACKDROP_WIDTH, backdrop_row(-80), LINE)
    canvas.fill(0, backdrop_row(-78) - 2, BACKDROP_WIDTH, 4, INK)
    return canvas.png()


def compact(value):
    """Writes a JSON value on one line, as the hand-written test content does."""
    if isinstance(value, dict):
        members = ", ".join(f"{json.dumps(key)}: {compact(item)}" for key, item in value.items())
        return f"{{ {members} }}"
    if isinstance(value, list):
        return "[" + ", ".join(compact(item) for item in value) + "]"
    return json.dumps(value)


def material(asset):
    return {
        "$schema": "../../../../schema/material.schema.json",
        "format": "wulfhilde-material",
        "version": 1,
        "color": {"texture": asset},
        "roughness": {"factor": 0.8},
        "metallic": {"factor": 0},
        "size": [METRES, METRES],
    }


def backdrop(asset, nits):
    return {
        "$schema": "../../../schema/backdrop.schema.json",
        "format": "wulfhilde-backdrop",
        "version": 1,
        "image": asset,
        "nits": nits,
    }


def write_json(path, value):
    members = ",\n".join(f"  {json.dumps(key)}: {compact(item)}" for key, item in value.items())
    path.write_text("{\n" + members + "\n}\n", encoding="utf-8", newline="\n")


def main():
    GRID.mkdir(parents=True, exist_ok=True)
    for role, name, background in ROLES:
        asset = f"materials/grid/{role}"
        (GRID / f"{role}.png").write_bytes(grid_texture(name, background))
        write_json(GRID / f"{role}.material.json", material(asset))

    BACKDROPS.mkdir(parents=True, exist_ok=True)
    for name, tint, nits in TINTS:
        (BACKDROPS / f"{name}.png").write_bytes(backdrop_image(tint))
        write_json(BACKDROPS / f"{name}.backdrop.json", backdrop(f"backdrops/{name}", nits))


if __name__ == "__main__":
    main()
