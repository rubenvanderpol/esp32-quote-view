/**
 * LilyGO T5-4.7" E-Paper S3 — quote viewer
 *
 * Shows quotes from LittleFS in file order, advancing automatically every
 * QUOTE_INTERVAL_HOURS (see config.h). Press the side button to skip ahead.
 */

#ifndef BOARD_HAS_PSRAM
#error "Enable OPI PSRAM in board settings"
#endif

#include <Arduino.h>
#include <Button2.h>

#include "config.h"
#include "quote_display.h"
#include "quote_scheduler.h"
#include "quote_store.h"
#include "utilities.h"

namespace {

QuoteStore g_store;
QuoteDisplay g_display;
QuoteScheduler g_scheduler;
Button2 g_button(BUTTON_1);

uint32_t g_last_check_ms = 0;

void showQuoteAt(size_t index) {
    QuoteRecord record;
    if (!g_store.get(index, record)) {
        Serial.printf("Failed to read quote %u\n", static_cast<unsigned>(index));
        return;
    }

    Serial.printf("[%u] %s — %s\n", static_cast<unsigned>(index), record.source.c_str(),
                  record.quote.c_str());
    g_display.show(record, index, g_store.count(), g_scheduler.secondsUntilNext());
}

void showCurrentQuote() {
    showQuoteAt(g_scheduler.currentIndex());
}

void onButtonPressed(Button2 &btn) {
    (void)btn;
    if (g_store.count() == 0) {
        return;
    }

    g_scheduler.skip(g_store.count());
    showCurrentQuote();
}

void checkSchedule() {
    if (g_store.count() == 0) {
        return;
    }

    if (g_scheduler.syncToClock(g_store.count())) {
        showCurrentQuote();
        return;
    }

    if (g_scheduler.due()) {
        g_scheduler.advance(g_store.count());
        showCurrentQuote();
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.printf("esp32-quote-view starting (interval %d h)\n", QUOTE_INTERVAL_HOURS);

    if (!g_scheduler.begin()) {
        Serial.println("Scheduler init failed");
        return;
    }

    if (!g_store.begin()) {
        Serial.println("Quote store init failed");
        return;
    }

    if (!g_display.begin()) {
        Serial.println("Display init failed");
        return;
    }

    g_button.setPressedHandler(onButtonPressed);
    g_scheduler.syncToClock(g_store.count());
    showCurrentQuote();
}

void loop() {
    g_button.loop();

    if (millis() - g_last_check_ms >= 30000UL) {
        g_last_check_ms = millis();
        checkSchedule();
    }

    delay(2);
}
