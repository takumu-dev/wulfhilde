#!/usr/bin/env python3
"""Writes the test content's debug-grid materials, one for each role a surface plays.

Each material's colour texture is a 512 px PNG covering 1 m, ruled every metre and every 0.25 m,
with arrows along +u and +v from the corner where u and v start, and the role's name, so that how a
surface lies, which way, how large and where it starts can be told at a glance. Every role has a
colour of its own.

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

# A font of 5 by 7 cells, each glyph its rows from the top, with only the characters the textures
# need.
FONT = {
    " ": ["00000"] * 7,
    "+": ["00000", "00100", "00100", "11111", "00100", "00100", "00000"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
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
}


class Canvas:
    """An RGB image addressed from its bottom-left corner, x to the right and y up, as u and v
    run."""

    def __init__(self, size, color):
        self.size = size
        self.pixels = [list(color) * size for _ in range(size)]

    def fill(self, left, bottom, width, height, color):
        """Fills a rectangle, wrapping around the edges, since the texture repeats."""
        for y in range(bottom, bottom + height):
            row = self.pixels[self.size - 1 - (y % self.size)]
            for x in range(left, left + width):
                start = 3 * (x % self.size)
                row[start : start + 3] = color

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
        header = struct.pack(">IIBBBBB", self.size, self.size, 8, 2, 0, 0, 0)
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
    canvas = Canvas(SIZE, background)
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


def main():
    GRID.mkdir(parents=True, exist_ok=True)
    for role, name, background in ROLES:
        asset = f"materials/grid/{role}"
        (GRID / f"{role}.png").write_bytes(grid_texture(name, background))
        members = ",\n".join(
            f"  {json.dumps(key)}: {compact(value)}" for key, value in material(asset).items()
        )
        text = "{\n" + members + "\n}\n"
        (GRID / f"{role}.material.json").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
