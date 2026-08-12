#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

class QuoteScheduler {
public:
    bool begin();
    size_t currentIndex() const { return index_; }
    uint32_t intervalSeconds() const { return interval_seconds_; }

    // Recompute index from elapsed intervals (e.g. after long power-off).
    bool syncToClock(size_t quote_count);

    // True when the next quote should be shown.
    bool due() const;

    // Move to the next quote in file order and schedule the following switch.
    void advance(size_t quote_count);

    // Skip ahead immediately (button); resets the interval from now.
    void skip(size_t quote_count);

    uint32_t secondsUntilNext() const;

private:
    bool initRtc();
    bool ensureRtcValid();
    bool readUnixTime(time_t *out) const;
    void loadState();
    void saveState();

    bool rtc_ok_ = false;
    size_t index_ = 0;
    time_t next_advance_ = 0;
    uint32_t interval_seconds_ = 0;
};
