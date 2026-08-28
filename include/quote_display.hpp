#pragma once

#include "quote_record.hpp"

#include <cstdint>

class QuoteDisplay {
public:
    bool begin();
    // Next show() holds black then white before drawing, so a leftover image
    // (old firmware layout, USB session, interrupted refresh) is erased.
    void requestFullScrub();
    void show(const QuoteRecord &quote);
    void powerOff();

private:
    void present();

    std::uint8_t *framebuffer_ = nullptr;
    bool full_scrub_next_ = false;
};
