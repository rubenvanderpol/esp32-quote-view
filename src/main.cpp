/**
 * LilyGO T5-4.7" E-Paper S3 — quote viewer
 *
 * Quotes are stored as a compact binary file on LittleFS (see scripts/pack_quotes.py).
 * Press the side button (GPIO 21) to show the next quote.
 */

#ifndef BOARD_HAS_PSRAM
#error "Enable OPI PSRAM in board settings"
#endif

#include <Arduino.h>
#include <Button2.h>
#include <Preferences.h>

#include "quote_display.h"
#include "quote_store.h"
#include "utilities.h"

namespace {

constexpr char kPrefsNamespace[] = "quoteview";
constexpr char kPrefsIndexKey[] = "index";

QuoteStore g_store;
QuoteDisplay g_display;
Button2 g_button(BUTTON_1);
Preferences g_prefs;

size_t g_index = 0;

void saveIndex() {
    g_prefs.putUInt(kPrefsIndexKey, static_cast<uint32_t>(g_index));
}

void showCurrentQuote() {
    QuoteRecord record;
    if (!g_store.get(g_index, record)) {
        Serial.printf("Failed to read quote %u\n", static_cast<unsigned>(g_index));
        return;
    }

    Serial.printf("[%u] %s — %s\n", static_cast<unsigned>(g_index), record.source.c_str(),
                  record.quote.c_str());
    g_display.show(record, g_index, g_store.count());
}

void onButtonPressed(Button2 &btn) {
    (void)btn;
    if (g_store.count() == 0) {
        return;
    }

    g_index = (g_index + 1) % g_store.count();
    saveIndex();
    showCurrentQuote();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("esp32-quote-view starting");

    g_prefs.begin(kPrefsNamespace, false);
    g_index = g_prefs.getUInt(kPrefsIndexKey, 0);

    if (!g_store.begin()) {
        Serial.println("Quote store init failed");
        return;
    }

    if (!g_display.begin()) {
        Serial.println("Display init failed");
        return;
    }

    if (g_index >= g_store.count()) {
        g_index = 0;
    }

    g_button.setPressedHandler(onButtonPressed);
    showCurrentQuote();
}

void loop() {
    g_button.loop();
    delay(2);
}
