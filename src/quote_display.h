#pragma once

#include "quote_store.h"

#include <stdint.h>

class QuoteDisplay {
public:
    bool begin();
    void show(const QuoteRecord &quote, size_t index, size_t total,
              uint32_t seconds_until_next = 0);
    void powerOff();

private:
    void drawWrappedText(const char *text, int32_t x, int32_t y, int32_t max_width,
                         int32_t line_height, uint8_t *framebuffer);
    int32_t measureLineWidth(const char *start, const char *end);

    uint8_t *framebuffer_ = nullptr;
};
