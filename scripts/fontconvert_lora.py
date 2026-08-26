#!/usr/bin/env python3
"""Convert Lora TTF to GFXfont headers for the LilyGO e-paper driver."""

import math
import sys
import zlib
from collections import namedtuple
from pathlib import Path

import freetype

FONT_PATH = Path(__file__).resolve().parents[1] / "assets" / "fonts" / "Lora-Regular.ttf"
OUTPUT_PATH = Path(__file__).resolve().parents[1] / "include" / "lora.h"
# Largest first in fonts.hpp; quote layout picks the biggest that fits.
# 18/16 pt cover long verses that overflow 22 pt on the 960x540 panel.
FONT_SIZES = (48, 36, 28, 22, 18, 16)
UI_SIZE = 28

# ASCII, Latin-1 letters/punctuation (é, ë, í, ü, …), plus quote marks.
# U+00AD (soft hyphen) is omitted: Lora has no glyph and intervals must be dense.
INTERVALS = [
    (32, 126),
    (0x00A0, 0x00AC),
    (0x00AE, 0x00FF),
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


def glyph_label(code_point: int) -> str:
    if 32 <= code_point < 127:
        # A trailing '\' continues the next source line, which would drop
        # the following glyph from the array (seen as Hello → Hfmmp).
        return "backslash" if code_point == 0x5C else chr(code_point)
    return f"U+{code_point:04X}"


def convert_size(face: freetype.Face, point_size: int):
    face.set_char_size(point_size << 6, point_size << 6, 150, 150)

    total_size = 0
    total_packed = 0
    glyph_data = []
    glyph_props = []

    for start, end in INTERVALS:
        for code_point in range(start, end + 1):
            glyph_index = face.get_char_index(code_point)
            if glyph_index == 0:
                print(
                    f"warning: U+{code_point:04X} missing from {FONT_PATH.name}; using .notdef",
                    file=sys.stderr,
                )

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

            glyph_props.append(
                GlyphProps(
                    width=bitmap.width,
                    height=bitmap.rows,
                    advance_x=norm_floor(face.glyph.advance.x),
                    left=face.glyph.bitmap_left,
                    top=face.glyph.bitmap_top,
                    compressed_size=len(compressed),
                    data_offset=total_size,
                    code_point=code_point,
                )
            )
            total_size += len(compressed)
            glyph_data.extend(compressed)

    metrics = {
        "advance_y": norm_ceil(face.size.height),
        "ascender": norm_ceil(face.size.ascender),
        "descender": norm_floor(face.size.descender),
    }
    return glyph_data, glyph_props, metrics, total_packed


def emit_bytes(name: str, data: list[int]) -> list[str]:
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for chunk in chunks(data, 16):
        lines.append("    " + " ".join(f"0x{byte:02X}," for byte in chunk))
    lines.append("};")
    return lines


def emit_glyphs(name: str, glyph_props) -> list[str]:
    lines = [f"const GFXglyph {name}[] = {{"]
    for glyph in glyph_props:
        lines.append(
            "    { "
            + ", ".join(str(value) for value in glyph[:-1])
            + "},"
            + f" // {glyph_label(glyph.code_point)}"
        )
    lines.append("};")
    return lines


def main() -> int:
    if not FONT_PATH.exists():
        print(f"Missing font file: {FONT_PATH}", file=sys.stderr)
        return 1

    face = freetype.Face(str(FONT_PATH))
    converted = {}
    for size in FONT_SIZES:
        converted[size] = convert_size(face, size)

    lines = [
        "#pragma once",
        "#include \"epd_driver.h\"",
        "",
        "const UnicodeInterval LoraIntervals[] = {",
    ]
    offset = 0
    for start, end in INTERVALS:
        lines.append(f"    {{ 0x{start:X}, 0x{end:X}, 0x{offset:X} }},")
        offset += end - start + 1
    lines.append("};")
    lines.append("")

    for size in FONT_SIZES:
        glyph_data, glyph_props, metrics, _packed = converted[size]
        prefix = f"Lora{size}"
        lines.extend(emit_bytes(f"{prefix}Bitmaps", glyph_data))
        lines.append("")
        lines.extend(emit_glyphs(f"{prefix}Glyphs", glyph_props))
        lines.append("")
        lines.append(f"const GFXfont {prefix} = {{")
        lines.append(f"    (uint8_t*){prefix}Bitmaps,")
        lines.append(f"    (GFXglyph*){prefix}Glyphs,")
        lines.append("    (UnicodeInterval*)LoraIntervals,")
        lines.append(f"    {len(INTERVALS)},")
        lines.append("    1,")
        lines.append(f"    {metrics['advance_y']},")
        lines.append(f"    {metrics['ascender']},")
        lines.append(f"    {metrics['descender']},")
        lines.append("};")
        lines.append("")

    lines.append(f"const GFXfont Lora = Lora{UI_SIZE};")
    lines.append("")

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    parts = ", ".join(f"{size}pt={len(converted[size][0])}B" for size in FONT_SIZES)
    print(f"Wrote {OUTPUT_PATH} ({parts})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
