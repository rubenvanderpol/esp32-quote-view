/**
 * Firmware entry point. Arduino requires setup()/loop(); application logic lives in QuoteApp.
 */

#ifndef BOARD_HAS_PSRAM
#error "Enable OPI PSRAM in board settings"
#endif

#include <Arduino.h>

#include "app.hpp"

namespace {

QuoteApp g_app;

}  // namespace

void setup() {
    g_app.setup();
}

void loop() {
    g_app.loop();
}
