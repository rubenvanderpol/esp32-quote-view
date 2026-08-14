#pragma once

#include <cstddef>
#include <cstdint>

class QuoteApp {
public:
    void setup();
    void loop();
    void onButtonPressed();

private:
    void showQuoteAt(std::size_t index);
    void showCurrentQuote();
    void checkSchedule();
    // Handles the wake reason and syncs the schedule; returns true if the
    // display needs a redraw.
    bool handleWakeCause();
    void enterDeepSleep();

    std::uint32_t last_check_ms_ = 0;
    bool initialized_ = false;
};
