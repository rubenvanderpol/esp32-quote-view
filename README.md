# esp32-quote-view

A quote-of-the-day display for the **LilyGO T5-4.7" E-Paper S3** (ESP32-S3, 960×540 grayscale e-ink). Quotes are stored in a compact binary format on the board's flash filesystem.

## Hardware

- [LilyGO T5-4.7-S3 E-Paper](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (4.7" ED047TC1, JST-PH Li-Po)
- USB-C cable for power, upload, serial monitor, and JTAG debug
- Side button (GPIO 21) cycles to the next quote

## Storage design

Human-editable source: `data/quotes.json`

At build time, `scripts/pack_quotes.py` converts this to `data/quotes.bin`:

| Technique | Why |
|-----------|-----|
| **Binary blob** vs JSON on device | No parse overhead; ~40–60% smaller than JSON for the same content |
| **Topic dictionary** | Topics stored once; each quote stores a 1-byte topic id |
| **Length-prefixed strings** | Variable-length quote/source without delimiters or padding |
| **Offset table** | O(1) random access to any quote by index |
| **LittleFS** | Wear-friendly filesystem in spare flash (default 16 MB partition) |
| **NVS (`Preferences`)** | Persists current quote index across reboots (few bytes) |

### Binary layout

```
Header (9 bytes): magic "QTE1", version, quote_count, topic_count
Topic table:      null-terminated UTF-8 strings
Offset table:     uint32 per quote
String pool:      [topic_id:u8][quote_len:u16][source_len:u8][quote][source] ...
```

## Quick start (VS Code + PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension.
2. Open this folder in VS Code.
3. Connect the board via USB-C.
4. Build and upload firmware:

   ```bash
   pio run -e T5-ePaper-S3 -t upload
   ```

5. Upload the quote database to flash:

   ```bash
   pio run -e T5-ePaper-S3 -t uploadfs
   ```

6. Open the serial monitor (115200 baud) to see log output:

   ```bash
   pio device monitor -e T5-ePaper-S3
   ```

Press the side button to advance to the next quote.

## Editing quotes

1. Edit `data/quotes.json` (add topics to the `topics` array first, then reference them in quotes).
2. Rebuild — the pack script runs automatically — and re-upload the filesystem:

   ```bash
   pio run -e T5-ePaper-S3 -t uploadfs
   ```

You can also pack manually:

```bash
python3 scripts/pack_quotes.py
```

## Debugging from VS Code

Yes — the ESP32-S3 on this board exposes **USB Serial/JTAG**, so you can debug from VS Code without an external probe.

### Setup

1. Use the `T5-ePaper-S3` environment (already set as default). It enables:
   - `debug_tool = esp-builtin` (on-chip JTAG)
   - `build_type = debug` (symbols, no aggressive optimisation)
   - `debug_init_break = tbreak setup` (pause at `setup()`)

2. In VS Code, open **Run and Debug** (Ctrl+Shift+D).

3. Select **PIO Debug (T5-ePaper-S3)** and press F5.

   PlatformIO will build, upload, and attach GDB. Set breakpoints in `src/main.cpp`, `quote_store.cpp`, or `quote_display.cpp`.

### Tips

- If the debug port is not found, check that **USB Mode** is *Hardware CDC and JTAG* (PlatformIO board profile handles this).
- Serial `Serial.println()` still works over the same USB connection while debugging.
- For everyday iteration, serial logging is often faster than full JTAG sessions on e-paper projects (each refresh takes ~1 s).

## Project layout

```
├── boards/T5-ePaper-S3.json   # LilyGO board definition
├── data/quotes.json           # Edit quotes here
├── scripts/pack_quotes.py     # JSON → binary packer
├── src/
│   ├── main.cpp               # Button + app loop
│   ├── quote_store.cpp        # LittleFS binary reader
│   └── quote_display.cpp      # E-paper layout
└── platformio.ini
```

## Dependencies

- [LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (`esp32s3` branch) — display driver
- [Button2](https://github.com/LennartHennigs/Button2) — debounced button

## Licence

Application code: MIT. LilyGo-EPD47 library: see upstream repository.
