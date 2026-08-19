#include "quote_scheduler.hpp"

#include <Arduino.h>
#include <Preferences.h>
#include <SensorPCF8563.hpp>
#include <Wire.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "config.hpp"
#include "utilities.h"

namespace {

constexpr char kPrefsNamespace[] = "quoteview";
constexpr char kPrefsIndexKey[] = "index";
constexpr char kPrefsNextKey[] = "next_adv";

// 2024-01-01 UTC. Timestamps below this are impossible (the RTC validity
// check uses the same year), so treat persisted values below it as unset.
constexpr std::time_t kMinValidTime = 1704067200;

Preferences g_prefs;
SensorPCF8563 g_rtc;

int parseCompileMonth(const char *month) {
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int i = 0; i < 12; ++i) {
        if (std::strncmp(month, names[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1;
}

std::time_t compileTimeUnix() {
    char date[16] = {};
    char time_str[16] = {};
    std::strncpy(date, __DATE__, sizeof(date) - 1);
    std::strncpy(time_str, __TIME__, sizeof(time_str) - 1);

    char month[4] = {};
    int day = 1;
    int year = 2026;
    std::sscanf(date, "%3s %d %d", month, &day, &year);

    int hour = 0;
    int minute = 0;
    int second = 0;
    std::sscanf(time_str, "%d:%d:%d", &hour, &minute, &second);

    std::tm tm_value = {};
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = parseCompileMonth(month) - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    return std::mktime(&tm_value);
}

}  // namespace

bool QuoteScheduler::begin() {
    interval_seconds_ = static_cast<std::uint32_t>(QUOTE_INTERVAL_SECONDS);
    g_prefs.begin(kPrefsNamespace, false);
    loadState();
    rtc_ok_ = initRtc();
    if (rtc_ok_) {
        ensureRtcValid();
        // A previous 6-hour build can leave a deadline hours away; clamp so a
        // shorter test interval takes effect on the next cycle.
        std::time_t now = 0;
        if (readUnixTime(&now) && next_advance_ > now + static_cast<std::time_t>(interval_seconds_)) {
            next_advance_ = now + static_cast<std::time_t>(interval_seconds_);
            saveState();
        }
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
    std::tm now = {};
    g_rtc.getDateTime(&now);

    if (now.tm_year + 1900 >= 2024) {
        return true;
    }

    const std::time_t compiled = compileTimeUnix();
    std::tm *compiled_tm = std::localtime(&compiled);
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

bool QuoteScheduler::readUnixTime(std::time_t *out) const {
    if (!rtc_ok_ || out == nullptr) {
        return false;
    }

    std::tm now = {};
    g_rtc.getDateTime(&now);

    if (now.tm_year + 1900 < 2024) {
        return false;
    }

    *out = std::mktime(&now);
    return *out > 0;
}

void QuoteScheduler::loadState() {
    index_ = g_prefs.getUInt(kPrefsIndexKey, 0);
    next_advance_ = static_cast<std::time_t>(g_prefs.getULong(kPrefsNextKey, 0));
    if (next_advance_ != 0 && next_advance_ < kMinValidTime) {
        // Impossible timestamp (e.g. leftover from an older firmware); reschedule from scratch
        // instead of "catching up" across decades of phantom intervals.
        next_advance_ = 0;
    }
}

void QuoteScheduler::saveState() {
    // next_advance_ is narrowed to uint32 (Preferences has no 64-bit slot); good until 2106.
    g_prefs.putUInt(kPrefsIndexKey, static_cast<std::uint32_t>(index_));
    g_prefs.putULong(kPrefsNextKey, static_cast<std::uint32_t>(next_advance_));
}

bool QuoteScheduler::syncToClock(std::size_t quote_count) {
    if (quote_count == 0) {
        return false;
    }

    if (index_ >= quote_count) {
        index_ = 0;
    }

    std::time_t now = 0;
    if (!readUnixTime(&now)) {
        return false;
    }

    if (next_advance_ == 0) {
        next_advance_ = now + static_cast<std::time_t>(interval_seconds_);
        saveState();
        return false;
    }

    if (now < next_advance_) {
        return false;
    }

    // Catch up on every interval that elapsed while powered off.
    const std::time_t interval = static_cast<std::time_t>(interval_seconds_);
    const std::time_t missed = (now - next_advance_) / interval + 1;
    index_ = (index_ + static_cast<std::size_t>(missed % static_cast<std::time_t>(quote_count)))
             % quote_count;
    next_advance_ += missed * interval;
    saveState();
    return true;
}

bool QuoteScheduler::due() const {
    if (next_advance_ == 0) {
        return false;
    }

    std::time_t now = 0;
    if (!readUnixTime(&now)) {
        return false;
    }

    return now >= next_advance_;
}

void QuoteScheduler::advance(std::size_t quote_count) {
    if (quote_count == 0) {
        return;
    }

    index_ = (index_ + 1) % quote_count;

    std::time_t now = 0;
    if (readUnixTime(&now)) {
        next_advance_ = now + static_cast<std::time_t>(interval_seconds_);
    } else {
        next_advance_ += static_cast<std::time_t>(interval_seconds_);
    }

    saveState();
}

void QuoteScheduler::skip(std::size_t quote_count) {
    advance(quote_count);
}

std::uint32_t QuoteScheduler::secondsUntilNext() const {
    std::time_t now = 0;
    if (!readUnixTime(&now) || next_advance_ <= now) {
        return 0;
    }
    return static_cast<std::uint32_t>(next_advance_ - now);
}
