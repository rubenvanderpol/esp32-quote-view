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

// Keep in sync with scripts/check_quote_layout.py.
constexpr std::int32_t kMarginX = 40;
constexpr std::int32_t kContentWidth = EPD_WIDTH - (kMarginX * 2);
constexpr std::int32_t kTopicTop = 28;
constexpr std::int32_t kTopicHeight = 52;
constexpr std::int32_t kTopicBottom = kTopicTop + kTopicHeight;
constexpr std::int32_t kQuoteTopGap = 24;
// Font advance_y is ascender+descender with almost no extra gap; pad so
// descenders (y, g, p, ë) do not collide with the next line's ascenders.
constexpr std::int32_t kLinePadding = 12;
// Sit the attribution near the bottom with room for 28 pt descenders.
constexpr std::int32_t kAttributionY = EPD_HEIGHT - 56;

std::int32_t fontDescender(const GFXfont *font) {
    return font->descender < 0 ? -font->descender : font->descender;
}

std::int32_t lineHeight(const GFXfont *font) {
    return static_cast<std::int32_t>(font->advance_y) + kLinePadding;
}

std::int32_t bodyStartBaseline(const GFXfont *font) {
    return kTopicBottom + kQuoteTopGap + font->ascender;
}

std::int32_t bodyMaxBaseline(const GFXfont *font) {
    return kAttributionY - font->ascender - fontDescender(font) - kLinePadding;
}

std::int32_t measureTextWidth(const GFXfont *font, const char *text) {
    if (!text || text[0] == '\0') {
        return 0;
    }
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
    std::int32_t w = 0;
    std::int32_t h = 0;
    get_text_bounds(font, text, &x, &y, &x1, &y1, &w, &h, nullptr);
    return w;
}

void drawText(const GFXfont *font, const char *text, std::int32_t x, std::int32_t y,
              std::uint8_t *framebuffer) {
    std::int32_t cursor_x = x;
    std::int32_t cursor_y = y;
    write_string(font, text, &cursor_x, &cursor_y, framebuffer);
}

// Greedy word wrap. Newlines force a break; words that exceed max_width on
// their own get a line of their own (the driver clips at the panel edge).
std::vector<std::string> wrapLines(const GFXfont *font, const char *text, std::int32_t max_width) {
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
            if (measureTextWidth(font, candidate.c_str()) > max_width) {
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

bool quoteFits(const GFXfont *font, const char *text) {
    if (!text || text[0] == '\0') {
        return true;
    }

    const std::vector<std::string> lines = wrapLines(font, text, kContentWidth);
    if (lines.empty()) {
        return true;
    }

    for (const std::string &line : lines) {
        if (measureTextWidth(font, line.c_str()) > kContentWidth) {
            return false;
        }
    }

    const std::int32_t start_y = bodyStartBaseline(font);
    const std::int32_t max_y = bodyMaxBaseline(font);
    if (start_y > max_y) {
        return false;
    }

    const std::int32_t last_y =
        start_y + static_cast<std::int32_t>(lines.size() - 1) * lineHeight(font);
    return last_y <= max_y;
}

const GFXfont *fontForQuote(const char *text) {
    for (std::size_t i = 0; i < kQuoteFontCount; ++i) {
        if (quoteFits(kQuoteFonts[i], text)) {
            return kQuoteFonts[i];
        }
    }
    return kQuoteFonts[kQuoteFontCount - 1];
}

void drawWrappedText(const GFXfont *font, const char *text, std::int32_t x, std::int32_t y,
                     std::int32_t max_width, std::int32_t line_height, std::int32_t max_y,
                     std::uint8_t *framebuffer) {
    if (!text || text[0] == '\0') {
        return;
    }

    const std::vector<std::string> lines = wrapLines(font, text, max_width);
    std::int32_t line_y = y;
    for (std::size_t i = 0; i < lines.size(); ++i, line_y += line_height) {
        const bool last_slot = line_y + line_height > max_y;
        if (last_slot && i + 1 < lines.size()) {
            drawText(font, (lines[i] + "...").c_str(), x, line_y, framebuffer);
            return;
        }
        drawText(font, lines[i].c_str(), x, line_y, framebuffer);
        if (last_slot) {
            return;
        }
    }
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

void QuoteDisplay::show(const QuoteRecord &quote) {
    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    const GFXfont *quote_font = fontForQuote(quote.quote.c_str());
    const std::int32_t start_y = bodyStartBaseline(quote_font);
    const std::int32_t mark_y = start_y - static_cast<std::int32_t>(quote_font->advance_y) / 2;

    const std::int32_t topic_width = measureTextWidth(kUiFont, quote.topic.c_str());
    epd_fill_rect(kMarginX, kTopicTop, topic_width + 32, kTopicHeight, 0x00, framebuffer_);
    FontProperties inverted = {
        .fg_color = 15,
        .bg_color = 0,
        .fallback_glyph = 0,
        .flags = 0,
    };
    std::int32_t topic_x = kMarginX + 16;
    std::int32_t topic_y = kTopicTop + 36;
    write_mode(kUiFont, quote.topic.c_str(), &topic_x, &topic_y, framebuffer_, WHITE_ON_BLACK,
               &inverted);

    drawText(quote_font, "\xE2\x80\x9C", kMarginX, mark_y, framebuffer_);
    drawWrappedText(quote_font, quote.quote.c_str(), kMarginX, start_y, kContentWidth,
                    lineHeight(quote_font), bodyMaxBaseline(quote_font), framebuffer_);

    char attribution[160];
    std::snprintf(attribution, sizeof(attribution), "\xE2\x80\x94 %s", quote.source.c_str());
    drawText(kUiFont, attribution, kMarginX, kAttributionY, framebuffer_);

    Serial.printf("quote face advance_y=%u\n", static_cast<unsigned>(quote_font->advance_y));

    epd_poweron();
    delay(10);
    epd_clear();
    epd_clear_area_cycles(epd_full_screen(), 4, 50);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
    epd_poweroff_all();
}

void QuoteDisplay::powerOff() {
    epd_poweroff_all();
}
