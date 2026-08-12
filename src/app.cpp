#include "app.hpp"

#include <Arduino.h>
#include <Button2.h>

#include <cstdio>

#include "config.hpp"
#include "quote_display.hpp"
#include "quote_scheduler.hpp"
#include "quote_store.hpp"
#include "utilities.h"

namespace {

QuoteStore g_store;
QuoteDisplay g_display;
QuoteScheduler g_scheduler;
Button2 g_button(BUTTON_1);
QuoteApp *g_app = nullptr;

void handleButtonPressed(Button2 &btn) {
    (void)btn;
    if (g_app != nullptr) {
        g_app->onButtonPressed();
    }
}

}  // namespace

void QuoteApp::showQuoteAt(std::size_t index) {
    QuoteRecord record;
    if (!g_store.get(index, record)) {
        Serial.printf("Failed to read quote %u\n", static_cast<unsigned>(index));
        return;
    }

    Serial.printf("[%u] %s — %s\n", static_cast<unsigned>(index), record.source.c_str(),
                  record.quote.c_str());
    g_display.show(record, index, g_store.count(), g_scheduler.secondsUntilNext());
}

void QuoteApp::showCurrentQuote() {
    showQuoteAt(g_scheduler.currentIndex());
}

void QuoteApp::onButtonPressed() {
    if (g_store.count() == 0) {
        return;
    }

    g_scheduler.skip(g_store.count());
    showCurrentQuote();
}

void QuoteApp::checkSchedule() {
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

void QuoteApp::setup() {
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

    g_app = this;
    g_button.setPressedHandler(handleButtonPressed);
    g_scheduler.syncToClock(g_store.count());
    showCurrentQuote();
    initialized_ = true;
}

void QuoteApp::loop() {
    if (!initialized_) {
        return;
    }

    g_button.loop();

    if (millis() - last_check_ms_ >= 30000UL) {
        last_check_ms_ = millis();
        checkSchedule();
    }

    delay(2);
}
