#pragma once

#include "quote_record.hpp"

#include <cstdint>

class QuoteDisplay {
public:
    bool begin();
    // Next present() runs the LilyGO screen-repair sequence (~40 s) so stacked
    // leftover quotes are erased before the new frame is drawn.
    void requestFullScrub();
    void show(const QuoteRecord &quote);
    void showMessage(const char *text);
    void powerOff();

private:
    void present();
    void runRepairSequence(const Rect_t &area);

    std::uint8_t *framebuffer_ = nullptr;
    bool full_scrub_next_ = false;
};
