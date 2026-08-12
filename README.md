# esp32-quote-view

C++17 firmware for the **LilyGO T5-4.7" E-Paper S3** (ESP32-S3, 960×540 grayscale e-ink). Application code is standard C++ (`*.cpp` / `*.hpp`); PlatformIO links it against the Arduino runtime because the LilyGO display driver requires it.

## Project layout

```
include/          C++ headers (app, store, scheduler, display, lora.h)
assets/fonts/     Lora-Regular.ttf (SIL Open Font License)
src/              C++ implementation + main.cpp entry point
tools/            Host C++ utility (pack_quotes.cpp)
data/quotes.json  Human-editable quote source
```

## Hardware

- [LilyGO T5-4.7-S3 E-Paper](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (4.7" ED047TC1, JST-PH Li-Po)
- USB-C cable for power, upload, serial monitor, and JTAG debug
- Side button (GPIO 21) skips to the next quote immediately
- On-board **PCF8563 RTC** drives automatic rotation every `QUOTE_INTERVAL_HOURS` (default 6 h)

## Storage design

Human-editable source: `data/quotes.json`

At build time, the host C++ tool `tools/pack_quotes.cpp` converts `data/quotes.json` into `data/quotes.bin`:

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

Press the side button to skip ahead. Otherwise the display advances to the **next quote in `quotes.json` order** every 6 hours (change `QUOTE_INTERVAL_HOURS` in `include/config.hpp`).

## Deep sleep (battery)

By default the firmware uses **deep sleep** between updates (`ENABLE_DEEP_SLEEP` in `include/config.hpp`). After drawing a quote it powers off the e-paper panel and sleeps until:

- the next scheduled quote change (RTC timer wake), or
- you press the side button (GPIO 21 wake).

On wake the ESP32 reboots, reloads the quote index from NVS flash, handles the wake reason, redraws, and sleeps again. LilyGO reports roughly **~388 µA** with timer + GPIO wake on this board.

Set `ENABLE_DEEP_SLEEP` to `0` while debugging over USB if you want continuous serial output and polled button handling instead of wake-on-press.

All text is rendered in **Lora** (28 px serif). To regenerate `include/lora.h` after changing the font or size, run `python3 scripts/fontconvert_lora.py` (requires `freetype-py`).

If the board was powered off, it catches up on boot (e.g. 18 hours off → skips ahead 3 quotes).

## Editing quotes

1. Edit `data/quotes.json` (add topics to the `topics` array first, then reference them in quotes).
2. Rebuild — the pack script runs automatically — and re-upload the filesystem:

   ```bash
   pio run -e T5-ePaper-S3 -t uploadfs
   ```

You can also pack manually:

```bash
g++ -std=c++17 -O2 tools/pack_quotes.cpp -o tools/pack_quotes
./tools/pack_quotes data/quotes.json data/quotes.bin
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

   PlatformIO will build, upload, and attach GDB. Set breakpoints in `src/app.cpp`, `quote_store.cpp`, or `quote_display.cpp`.

### Tips

- If the debug port is not found, check that **USB Mode** is *Hardware CDC and JTAG* (PlatformIO board profile handles this).
- Serial `Serial.println()` still works over the same USB connection while debugging.
- For everyday iteration, serial logging is often faster than full JTAG sessions on e-paper projects (each refresh takes ~1 s).

## Project layout

```
├── include/                   # C++ headers
├── tools/pack_quotes.cpp      # Host C++ JSON → binary packer
├── data/quotes.json           # Edit quotes here
├── src/
│   ├── main.cpp               # setup()/loop() entry
│   ├── app.cpp                # Application orchestration
│   ├── quote_scheduler.cpp    # RTC-based interval rotation
│   ├── quote_store.cpp        # LittleFS binary reader
│   └── quote_display.cpp      # E-paper layout
└── platformio.ini
```

## Dependencies

- [LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (`esp32s3` branch) — display driver
- [Button2](https://github.com/LennartHennigs/Button2) — debounced button

## Licence

Application code: MIT. LilyGo-EPD47 library: see upstream repository.
