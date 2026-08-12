#!/usr/bin/env python3
"""Pack quotes.json into data/quotes.bin (see module docstring in pack logic)."""

from __future__ import annotations

import json
import struct
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parents[1]
JSON_PATH = PROJECT_DIR / "data" / "quotes.json"
BIN_PATH = PROJECT_DIR / "data" / "quotes.bin"


def pack_quotes() -> None:
    payload = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    topics = payload["topics"]
    quotes = payload["quotes"]

    topic_index = {name: idx for idx, name in enumerate(topics)}

    topic_bytes = bytearray()
    for topic in topics:
        topic_bytes.extend(topic.encode("utf-8"))
        topic_bytes.append(0)

    pool = bytearray()
    offsets: list[int] = []

    for entry in quotes:
        topic_id = topic_index[entry["topic"]]
        quote = entry["quote"].encode("utf-8")
        source = entry["source"].encode("utf-8")

        if len(quote) > 0xFFFF:
            raise ValueError("quote too long")
        if len(source) > 255:
            raise ValueError("source too long")

        offsets.append(len(pool))
        pool.append(topic_id)
        pool.extend(struct.pack("<HB", len(quote), len(source)))
        pool.extend(quote)
        pool.extend(source)

    header = struct.pack("<4sBHBB", b"QTE1", 1, len(quotes), len(topics), 0)
    offset_bytes = b"".join(struct.pack("<I", off) for off in offsets)
    blob = header + bytes(topic_bytes) + offset_bytes + bytes(pool)

    BIN_PATH.parent.mkdir(parents=True, exist_ok=True)
    BIN_PATH.write_bytes(blob)
    print(f"Packed {len(quotes)} quotes ({len(blob)} bytes) -> {BIN_PATH}")


if __name__ == "__main__":
    pack_quotes()
