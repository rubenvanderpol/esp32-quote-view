#!/usr/bin/env python3
"""Convert Lora TTF to a GFXfont header for the LilyGO e-paper driver."""

import math
import sys
import zlib
from collections import namedtuple
from pathlib import Path

import freetype

FONT_PATH = Path(__file__).resolve().parents[1] / "assets" / "fonts" / "Lora-Regular.ttf"
OUTPUT_PATH = Path(__file__).resolve().parents[1] / "include" / "lora.h"
FONT_NAME = "Lora"
FONT_SIZE = 28

# ASCII plus typographic punctuation used in the quote UI.
INTERVALS = [
    (32, 126),
    (0x2013, 0x2014),  # en dash, em dash
    (0x2018, 0x2019),  # single quotes
    (0x201C, 0x201D),  # double quotes
]

GlyphProps = namedtuple(
    "GlyphProps",
    ["width", "height", "advance_x", "left", "top", "compressed_size", "data_offset", "code_point"],
)


def norm_floor(val):
    return int(math.floor(val / (1 << 6)))


def norm_ceil(val):
    return int(math.ceil(val / (1 << 6)))


def chunks(items, size):
    for index in range(0, len(items), size):
        yield items[index : index + size]


def main() -> int:
    if not FONT_PATH.exists():
        print(f"Missing font file: {FONT_PATH}", file=sys.stderr)
        return 1

    face = freetype.Face(str(FONT_PATH))
    face.set_char_size(FONT_SIZE << 6, FONT_SIZE << 6, 150, 150)

    total_size = 0
    total_packed = 0
    glyph_data = []
    glyph_props = []

    for start, end in INTERVALS:
        for code_point in range(start, end + 1):
            glyph_index = face.get_char_index(code_point)
            if glyph_index == 0:
                raise ValueError(f"code point U+{code_point:04X} missing from {FONT_PATH.name}")

            face.load_glyph(glyph_index, freetype.FT_LOAD_RENDER)
            bitmap = face.glyph.bitmap
            pixels = []
            px = 0
            for index, value in enumerate(bitmap.buffer):
                x = index % bitmap.width
                if x % 2 == 0:
                    px = value >> 4
                else:
                    px = px | (value & 0xF0)
                    pixels.append(px)
                if x == bitmap.width - 1 and bitmap.width % 2 > 0:
                    pixels.append(px)

            packed = bytes(pixels)
            total_packed += len(packed)
            compressed = zlib.compress(packed)

            glyph = GlyphProps(
                width=bitmap.width,
                height=bitmap.rows,
                advance_x=norm_floor(face.glyph.advance.x),
                left=face.glyph.bitmap_left,
                top=face.glyph.bitmap_top,
                compressed_size=len(compressed),
                data_offset=total_size,
                code_point=code_point,
            )
            total_size += len(compressed)
            glyph_props.append(glyph)
            glyph_data.extend(compressed)

    lines = [
        "#pragma once",
        "#include \"epd_driver.h\"",
        f"const uint8_t {FONT_NAME}Bitmaps[{len(glyph_data)}] = {{",
    ]
    for chunk in chunks(glyph_data, 16):
        lines.append("    " + " ".join(f"0x{byte:02X}," for byte in chunk))
    lines.append("};")
    lines.append("")
    lines.append(f"const GFXglyph {FONT_NAME}Glyphs[] = {{")
    for glyph in glyph_props:
        if 32 <= glyph.code_point < 127:
            # A trailing '\' continues the next source line, which would drop
            # the following glyph from the array (seen as Hello → Hfmmp).
            label = "backslash" if glyph.code_point == 0x5C else chr(glyph.code_point)
        else:
            label = f"U+{glyph.code_point:04X}"
        lines.append(
            "    { "
            + ", ".join(str(value) for value in glyph[:-1])
            + "},"
            + f" // {label}"
        )
    lines.append("};")
    lines.append("")
    lines.append(f"const UnicodeInterval {FONT_NAME}Intervals[] = {{")
    offset = 0
    for start, end in INTERVALS:
        lines.append(f"    {{ 0x{start:X}, 0x{end:X}, 0x{offset:X} }},")
        offset += end - start + 1
    lines.append("};")
    lines.append("")
    lines.append(f"const GFXfont {FONT_NAME} = {{")
    lines.append(f"    (uint8_t*){FONT_NAME}Bitmaps,")
    lines.append(f"    (GFXglyph*){FONT_NAME}Glyphs,")
    lines.append(f"    (UnicodeInterval*){FONT_NAME}Intervals,")
    lines.append(f"    {len(INTERVALS)},")
    lines.append("    1,")
    lines.append(f"    {norm_ceil(face.size.height)},")
    lines.append(f"    {norm_ceil(face.size.ascender)},")
    lines.append(f"    {norm_floor(face.size.descender)},")
    lines.append("};")
    lines.append("")

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUTPUT_PATH} ({len(glyph_data)} bytes bitmap data, {total_packed} unpacked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
