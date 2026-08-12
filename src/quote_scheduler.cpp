#include "quote_scheduler.h"

#include <Preferences.h>
#include <SensorPCF8563.hpp>
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "utilities.h"

// SensorLib header-only RTC uses this symbol from the base class.
constexpr uint8_t PCF8563Constants::PCF8563_SLAVE_ADDRESS;

namespace {

constexpr char kPrefsNamespace[] = "quoteview";
constexpr char kPrefsIndexKey[] = "index";
constexpr char kPrefsNextKey[] = "next_adv";

Preferences g_prefs;
SensorPCF8563 g_rtc;

int parseCompileMonth(const char *month) {
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int i = 0; i < 12; ++i) {
        if (strncmp(month, names[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1;
}

time_t compileTimeUnix() {
    char date[16] = {};
    char time_str[16] = {};
    strncpy(date, __DATE__, sizeof(date) - 1);
    strncpy(time_str, __TIME__, sizeof(time_str) - 1);

    char month[4] = {};
    int day = 1;
    int year = 2026;
    sscanf(date, "%3s %d %d", month, &day, &year);

    int hour = 0;
    int minute = 0;
    int second = 0;
    sscanf(time_str, "%d:%d:%d", &hour, &minute, &second);

    struct tm tm_value = {};
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = parseCompileMonth(month) - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    return mktime(&tm_value);
}

}  // namespace

bool QuoteScheduler::begin() {
    interval_seconds_ = static_cast<uint32_t>(QUOTE_INTERVAL_HOURS) * 3600U;
    g_prefs.begin(kPrefsNamespace, false);
    loadState();
    rtc_ok_ = initRtc();
    if (rtc_ok_) {
        ensureRtcValid();
    }
    return true;
}

bool QuoteScheduler::initRtc() {
    Wire.begin(BOARD_SDA, BOARD_SCL);
    if (!g_rtc.begin(Wire)) {
        Serial.println("RTC not found — timed rotation needs PCF8563");
        return false;
    }
    return true;
}

bool QuoteScheduler::ensureRtcValid() {
    struct tm now = {};
    g_rtc.getDateTime(&now);

    if (now.tm_year + 1900 >= 2024) {
        return true;
    }

    time_t compiled = compileTimeUnix();
    struct tm *compiled_tm = localtime(&compiled);
    if (!compiled_tm) {
        return false;
    }

    g_rtc.setDateTime(
        compiled_tm->tm_year + 1900,
        compiled_tm->tm_mon + 1,
        compiled_tm->tm_mday,
        compiled_tm->tm_hour,
        compiled_tm->tm_min,
        compiled_tm->tm_sec);

    Serial.println("RTC was unset; initialized from firmware build time");
    return true;
}

bool QuoteScheduler::readUnixTime(time_t *out) const {
    if (!rtc_ok_ || out == nullptr) {
        return false;
    }

    struct tm now = {};
    g_rtc.getDateTime(&now);

    if (now.tm_year + 1900 < 2024) {
        return false;
    }

    *out = mktime(&now);
    return *out > 0;
}

void QuoteScheduler::loadState() {
    index_ = g_prefs.getUInt(kPrefsIndexKey, 0);
    next_advance_ = static_cast<time_t>(g_prefs.getULong(kPrefsNextKey, 0));
}

void QuoteScheduler::saveState() {
    g_prefs.putUInt(kPrefsIndexKey, static_cast<uint32_t>(index_));
    g_prefs.putULong(kPrefsNextKey, static_cast<uint32_t>(next_advance_));
}

bool QuoteScheduler::syncToClock(size_t quote_count) {
    if (quote_count == 0) {
        return false;
    }

    if (index_ >= quote_count) {
        index_ = 0;
    }

    time_t now = 0;
    if (!readUnixTime(&now)) {
        if (next_advance_ == 0) {
            next_advance_ = 1;
            saveState();
        }
        return false;
    }

    if (next_advance_ == 0) {
        next_advance_ = now + static_cast<time_t>(interval_seconds_);
        saveState();
        return false;
    }

    bool advanced = false;
    while (now >= next_advance_ && quote_count > 0) {
        index_ = (index_ + 1) % quote_count;
        next_advance_ += static_cast<time_t>(interval_seconds_);
        advanced = true;
    }

    if (advanced) {
        saveState();
    }
    return advanced;
}

bool QuoteScheduler::due() const {
    if (next_advance_ == 0) {
        return false;
    }

    time_t now = 0;
    if (!readUnixTime(&now)) {
        return false;
    }

    return now >= next_advance_;
}

void QuoteScheduler::advance(size_t quote_count) {
    if (quote_count == 0) {
        return;
    }

    index_ = (index_ + 1) % quote_count;

    time_t now = 0;
    if (readUnixTime(&now)) {
        next_advance_ = now + static_cast<time_t>(interval_seconds_);
    } else {
        next_advance_ += static_cast<time_t>(interval_seconds_);
    }

    saveState();
}

void QuoteScheduler::skip(size_t quote_count) {
    advance(quote_count);
}

uint32_t QuoteScheduler::secondsUntilNext() const {
    time_t now = 0;
    if (!readUnixTime(&now) || next_advance_ <= now) {
        return 0;
    }
    return static_cast<uint32_t>(next_advance_ - now);
}
