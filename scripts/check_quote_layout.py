#!/usr/bin/env python3
"""Verify every quotes.json entry fits the 960x540 panel at some Lora size.

Uses the same wrap/fit rules and FreeType metrics as quote_display.cpp /
fontconvert_lora.py. Run after changing layout constants, font sizes, or quotes:

    python3 scripts/check_quote_layout.py
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import freetype

ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = ROOT / "assets" / "fonts" / "Lora-Regular.ttf"
QUOTES_PATH = ROOT / "data" / "quotes.json"

# Keep in sync with src/quote_display.cpp and scripts/fontconvert_lora.py.
EPD_WIDTH = 960
EPD_HEIGHT = 540
MARGIN_X = 40
CONTENT_WIDTH = EPD_WIDTH - (MARGIN_X * 2)
TOPIC_BOTTOM = 28 + 52
QUOTE_TOP_GAP = 24
LINE_PADDING = 12
ATTRIBUTION_Y = EPD_HEIGHT - 56
FONT_SIZES = (48, 36, 28, 22, 18, 16)


def norm_floor(val: int) -> int:
    return int(math.floor(val / (1 << 6)))


def norm_ceil(val: int) -> int:
    return int(math.ceil(val / (1 << 6)))


def font_metrics(face: freetype.Face, point_size: int) -> dict:
    face.set_char_size(point_size << 6, point_size << 6, 150, 150)
    return {
        "advance_y": norm_ceil(face.size.height),
        "ascender": norm_ceil(face.size.ascender),
        "descender": norm_floor(face.size.descender),
        "point_size": point_size,
    }


def glyph_advance(face: freetype.Face, code_point: int) -> int:
    index = face.get_char_index(code_point)
    face.load_glyph(index, freetype.FT_LOAD_DEFAULT)
    return norm_floor(face.glyph.advance.x)


def measure_text_width(face: freetype.Face, text: str) -> int:
    width = 0
    for ch in text:
        width += glyph_advance(face, ord(ch))
    return width


def wrap_lines(face: freetype.Face, text: str, max_width: int) -> list[str]:
    lines: list[str] = []
    current = ""
    word = ""

    def flush_word() -> None:
        nonlocal current, word
        if not word:
            return
        if not current:
            current = word
        else:
            candidate = current + " " + word
            if measure_text_width(face, candidate) > max_width:
                lines.append(current)
                current = word
            else:
                current = candidate
        word = ""

    for ch in text:
        if ch == " ":
            flush_word()
        elif ch == "\n":
            flush_word()
            lines.append(current)
            current = ""
        else:
            word += ch
    flush_word()
    if current:
        lines.append(current)
    return lines


def quote_fits(face: freetype.Face, metrics: dict, text: str) -> bool:
    if not text:
        return True

    face.set_char_size(metrics["point_size"] << 6, metrics["point_size"] << 6, 150, 150)
    lines = wrap_lines(face, text, CONTENT_WIDTH)
    if not lines:
        return True

    for line in lines:
        if measure_text_width(face, line) > CONTENT_WIDTH:
            return False

    descender = -metrics["descender"] if metrics["descender"] < 0 else metrics["descender"]
    line_height = metrics["advance_y"] + LINE_PADDING
    start_y = TOPIC_BOTTOM + QUOTE_TOP_GAP + metrics["ascender"]
    max_y = ATTRIBUTION_Y - metrics["ascender"] - descender - LINE_PADDING
    if start_y > max_y:
        return False
    last_y = start_y + (len(lines) - 1) * line_height
    return last_y <= max_y


def font_for_quote(face: freetype.Face, metrics_by_size: dict, text: str) -> int | None:
    for size in FONT_SIZES:
        if quote_fits(face, metrics_by_size[size], text):
            return size
    return None


def main() -> int:
    face = freetype.Face(str(FONT_PATH))
    metrics_by_size = {size: font_metrics(face, size) for size in FONT_SIZES}
    doc = json.loads(QUOTES_PATH.read_text(encoding="utf-8"))

    failed = []
    print(f"{'pt':>4}  {'chars':>5}  quote")
    for entry in doc["quotes"]:
        text = entry["quote"]
        size = font_for_quote(face, metrics_by_size, text)
        if size is None:
            failed.append(text)
            print(f"{'FAIL':>4}  {len(text):5d}  {text}")
        else:
            print(f"{size:4d}  {len(text):5d}  {text[:72]}{'…' if len(text) > 72 else ''}")

    if failed:
        print(f"\n{len(failed)} quote(s) do not fit at {FONT_SIZES[-1]} pt", file=sys.stderr)
        return 1
    print(f"\nAll {len(doc['quotes'])} quotes fit (smallest face {FONT_SIZES[-1]} pt).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
