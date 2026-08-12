#include "app.hpp"

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>

#include <cstdio>

#include "config.hpp"
#include "quote_display.hpp"
#include "quote_scheduler.hpp"
#include "quote_store.hpp"
#include "utilities.h"

#if !ENABLE_DEEP_SLEEP
#include <Button2.h>
#endif

namespace {

QuoteStore g_store;
QuoteDisplay g_display;
QuoteScheduler g_scheduler;

#if !ENABLE_DEEP_SLEEP
Button2 g_button(BUTTON_1);
QuoteApp *g_app = nullptr;

void handleButtonPressed(Button2 &btn) {
    (void)btn;
    if (g_app != nullptr) {
        g_app->onButtonPressed();
    }
}
#endif

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

void QuoteApp::handleWakeCause() {
    if (g_store.count() == 0) {
        return;
    }

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wake: button — skipping to next quote");
            g_scheduler.skip(g_store.count());
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wake: timer — checking schedule");
            break;
        default:
            Serial.println("Wake: cold boot");
            break;
    }
}

void QuoteApp::enterDeepSleep() {
    g_display.powerOff();

    std::uint32_t sleep_seconds = g_scheduler.secondsUntilNext();
    if (sleep_seconds == 0) {
        sleep_seconds = g_scheduler.intervalSeconds();
    }

    const std::uint64_t sleep_us = static_cast<std::uint64_t>(sleep_seconds) * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_sleep_enable_ext1_wakeup(_BV(GPIO_NUM_21), ESP_EXT1_WAKEUP_ANY_LOW);

    Serial.printf("Deep sleep for %u s (index %u)\n", sleep_seconds,
                  static_cast<unsigned>(g_scheduler.currentIndex()));
    Serial.flush();
    delay(100);

    Wire.end();
    esp_deep_sleep_start();
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

#if !ENABLE_DEEP_SLEEP
    g_app = this;
    g_button.setPressedHandler(handleButtonPressed);
#endif

    handleWakeCause();
    g_scheduler.syncToClock(g_store.count());
    showCurrentQuote();
    initialized_ = true;

#if ENABLE_DEEP_SLEEP
    enterDeepSleep();
#endif
}

void QuoteApp::loop() {
    if (!initialized_) {
        return;
    }

#if !ENABLE_DEEP_SLEEP
    g_button.loop();

    if (millis() - last_check_ms_ >= 30000UL) {
        last_check_ms_ = millis();
        checkSchedule();
    }

    delay(2);
#endif
}
