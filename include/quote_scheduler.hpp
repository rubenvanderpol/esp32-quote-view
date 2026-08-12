#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

class QuoteScheduler {
public:
    bool begin();
    std::size_t currentIndex() const { return index_; }
    std::uint32_t intervalSeconds() const { return interval_seconds_; }

    bool syncToClock(std::size_t quote_count);
    bool due() const;
    void advance(std::size_t quote_count);
    void skip(std::size_t quote_count);
    std::uint32_t secondsUntilNext() const;

private:
    bool initRtc();
    bool ensureRtcValid();
    bool readUnixTime(std::time_t *out) const;
    void loadState();
    void saveState();

    bool rtc_ok_ = false;
    std::size_t index_ = 0;
    std::time_t next_advance_ = 0;
    std::uint32_t interval_seconds_ = 0;
};
