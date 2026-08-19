#include "quote_display.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "epd_driver.h"
#include "fonts.hpp"
#include "utilities.h"

namespace {

constexpr std::int32_t kMarginX = 48;
constexpr std::int32_t kContentWidth = EPD_WIDTH - (kMarginX * 2);
constexpr std::int32_t kBodyLineHeight = 46;
constexpr std::int32_t kBodyStartY = 205;
constexpr std::int32_t kAttributionY = EPD_HEIGHT - 150;
// Last baseline the quote body may occupy without colliding with the attribution.
constexpr std::int32_t kBodyMaxY = kAttributionY - kBodyLineHeight;

std::int32_t measureTextWidth(const char *text) {
    if (!text || text[0] == '\0') {
        return 0;
    }
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
    std::int32_t w = 0;
    std::int32_t h = 0;
    get_text_bounds(kDisplayFont, text, &x, &y, &x1, &y1, &w, &h, nullptr);
    return w;
}

void drawText(const char *text, std::int32_t x, std::int32_t y, std::uint8_t *framebuffer) {
    std::int32_t cursor_x = x;
    std::int32_t cursor_y = y;
    write_string(kDisplayFont, text, &cursor_x, &cursor_y, framebuffer);
}

void drawCentered(const char *text, std::int32_t y, std::uint8_t *framebuffer) {
    const std::int32_t width = measureTextWidth(text);
    std::int32_t x = (EPD_WIDTH - width) / 2;
    if (x < kMarginX) {
        x = kMarginX;
    }
    drawText(text, x, y, framebuffer);
}

// Greedy word wrap. Newlines force a break; words that exceed max_width on
// their own get a line of their own (the driver clips at the panel edge).
std::vector<std::string> wrapLines(const char *text, std::int32_t max_width) {
    std::vector<std::string> lines;
    std::string current;
    std::string word;

    const auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        if (current.empty()) {
            current = std::move(word);
        } else {
            std::string candidate = current + ' ' + word;
            if (measureTextWidth(candidate.c_str()) > max_width) {
                lines.push_back(std::move(current));
                current = std::move(word);
            } else {
                current = std::move(candidate);
            }
        }
        word.clear();
    };

    for (const char *p = text; *p; ++p) {
        if (*p == ' ') {
            flushWord();
        } else if (*p == '\n') {
            flushWord();
            lines.push_back(std::move(current));
            current.clear();
        } else {
            word.push_back(*p);
        }
    }
    flushWord();
    if (!current.empty()) {
        lines.push_back(std::move(current));
    }
    return lines;
}

}  // namespace

bool QuoteDisplay::begin() {
    epd_init();

    framebuffer_ = static_cast<std::uint8_t *>(
        ps_calloc(sizeof(std::uint8_t), EPD_WIDTH * EPD_HEIGHT / 2));
    if (!framebuffer_) {
        Serial.println("framebuffer alloc failed");
        return false;
    }

    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    return true;
}

void QuoteDisplay::drawWrappedText(const char *text, std::int32_t x, std::int32_t y,
                                   std::int32_t max_width, std::int32_t line_height,
                                   std::int32_t max_y, std::uint8_t *framebuffer) {
    if (!text || text[0] == '\0') {
        return;
    }

    const std::vector<std::string> lines = wrapLines(text, max_width);
    std::int32_t line_y = y;
    for (std::size_t i = 0; i < lines.size(); ++i, line_y += line_height) {
        const bool last_slot = line_y + line_height > max_y;
        if (last_slot && i + 1 < lines.size()) {
            drawText((lines[i] + "...").c_str(), x, line_y, framebuffer);
            return;
        }
        drawText(lines[i].c_str(), x, line_y, framebuffer);
        if (last_slot) {
            return;
        }
    }
}

void QuoteDisplay::show(const QuoteRecord &quote, std::size_t index, std::size_t total) {
    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    const std::int32_t topic_width = measureTextWidth(quote.topic.c_str());
    epd_fill_rect(kMarginX, 36, topic_width + 32, 52, 0x00, framebuffer_);
    FontProperties inverted = {
        .fg_color = 15,
        .bg_color = 0,
        .fallback_glyph = 0,
        .flags = 0,
    };
    std::int32_t topic_x = kMarginX + 16;
    std::int32_t topic_y = 72;
    write_mode(kDisplayFont, quote.topic.c_str(), &topic_x, &topic_y, framebuffer_,
               WHITE_ON_BLACK, &inverted);

    drawText("\xE2\x80\x9C", kMarginX, 148, framebuffer_);

    drawWrappedText(quote.quote.c_str(), kMarginX, kBodyStartY, kContentWidth, kBodyLineHeight,
                    kBodyMaxY, framebuffer_);

    char attribution[160];
    std::snprintf(attribution, sizeof(attribution), "\xE2\x80\x94 %s", quote.source.c_str());
    drawText(attribution, kMarginX, kAttributionY, framebuffer_);

    char footer[32];
    std::snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(index + 1),
                  static_cast<unsigned>(total));
    drawCentered(footer, EPD_HEIGHT - 70, framebuffer_);

    epd_draw_hline(kMarginX, EPD_HEIGHT - 110, kContentWidth, 0, framebuffer_);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
    epd_poweroff_all();
}

void QuoteDisplay::powerOff() {
    epd_poweroff_all();
}
