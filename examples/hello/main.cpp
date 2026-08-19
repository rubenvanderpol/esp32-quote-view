/**
 * Display smoke test: clear the panel and draw "Hello".
 * Does not use LittleFS or quotes.bin.
 *
 *   pio run -e hello -t upload
 */

#ifndef BOARD_HAS_PSRAM
#error "Enable OPI PSRAM in board settings"
#endif

#include <Arduino.h>
#include <cstdint>
#include <cstring>

#include "epd_driver.h"
#include "fonts.hpp"
#include "utilities.h"

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("hello: starting");

    epd_init();

    auto *framebuffer = static_cast<std::uint8_t *>(
        ps_calloc(sizeof(std::uint8_t), EPD_WIDTH * EPD_HEIGHT / 2));
    if (framebuffer == nullptr) {
        Serial.println("hello: framebuffer alloc failed");
        return;
    }
    std::memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    int cursor_x = 80;
    int cursor_y = 280;
    write_string(kDisplayFont, "Hello", &cursor_x, &cursor_y, framebuffer);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();

    Serial.println("hello: drawn");
}

void loop() {
    delay(1000);
}
