#pragma once

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
    void handleWakeCause();
    void enterDeepSleep();

    std::uint32_t last_check_ms_ = 0;
    bool initialized_ = false;
};
