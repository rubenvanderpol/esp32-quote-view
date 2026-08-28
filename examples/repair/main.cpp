/**
 * Full-screen afterimage repair (LilyGO screen_repair sequence).
 * Flashes the panel for ~30–40 s, then leaves it white.
 *
 *   pio run -e repair -t upload
 *
 * After a successful upload, tap RST (do not hold IO0) so the sketch
 * actually starts. Then wait until the panel is white.
 */

#ifndef BOARD_HAS_PSRAM
#error "Enable OPI PSRAM in board settings"
#endif

#include <Arduino.h>

#include "epd_driver.h"
#include "utilities.h"

void setup() {
    // Drive the panel first. USB serial can wait; the wipe must not.
    epd_init();

    const Rect_t area = epd_full_screen();
    epd_poweron();
    delay(10);
    epd_clear();
    for (int32_t i = 0; i < 20; i++) {
        epd_push_pixels(area, 50, 0);
        delay(500);
    }
    epd_clear();
    for (int32_t i = 0; i < 40; i++) {
        epd_push_pixels(area, 50, 1);
        delay(500);
    }
    epd_clear();
    epd_poweroff_all();

    Serial.begin(115200);
    Serial.println("repair: done — panel should be blank");
}

void loop() {
    delay(1000);
}
