#pragma once

#include "quote_record.hpp"

#include <cstddef>
#include <cstdint>

class QuoteDisplay {
public:
    bool begin();
    void show(const QuoteRecord &quote, std::size_t index, std::size_t total,
              std::uint32_t seconds_until_next = 0);
    void powerOff();

private:
    void drawWrappedText(const char *text, std::int32_t x, std::int32_t y, std::int32_t max_width,
                         std::int32_t line_height, std::uint8_t *framebuffer);
    std::int32_t measureLineWidth(const char *start, const char *end);

    std::uint8_t *framebuffer_ = nullptr;
};
