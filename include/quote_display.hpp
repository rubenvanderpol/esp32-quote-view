#pragma once

#include "quote_record.hpp"

#include <cstddef>
#include <cstdint>

class QuoteDisplay {
public:
    bool begin();
    void show(const QuoteRecord &quote);
    void powerOff();

private:
    void drawWrappedText(const char *text, std::int32_t x, std::int32_t y, std::int32_t max_width,
                         std::int32_t line_height, std::int32_t max_y, std::uint8_t *framebuffer);

    std::uint8_t *framebuffer_ = nullptr;
};
