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
constexpr std::int32_t kQuoteTop = 28;
// Font advance_y is ascender+descender with almost no extra gap; pad so
// descenders (y, g, p, ë) do not collide with the next line's ascenders.
constexpr std::int32_t kLinePadding = 12;
// Footer: source on the left, topic on the right, same baseline.
constexpr std::int32_t kFooterBaseline = EPD_HEIGHT - 36;
constexpr std::int32_t kFooterGap = 16;
constexpr std::int32_t kQuoteMarkGap = 8;
constexpr const char *kOpenQuote = "\xE2\x80\x9C";
constexpr const char *kCloseQuote = "\xE2\x80\x9D";

std::int32_t fontDescender(const GFXfont *font) {
    return font->descender < 0 ? -font->descender : font->descender;
}

std::int32_t lineHeight(const GFXfont *font) {
    return static_cast<std::int32_t>(font->advance_y) + kLinePadding;
}

std::int32_t bodyStartBaseline(const GFXfont *font) {
    return kQuoteTop + font->ascender;
}

std::int32_t bodyMaxBaseline(const GFXfont *font) {
    const std::int32_t source_asc = static_cast<std::int32_t>(kSourceFont->ascender);
    const std::int32_t topic_asc = static_cast<std::int32_t>(kTopicFont->ascender);
    const std::int32_t footer_ascender = source_asc > topic_asc ? source_asc : topic_asc;
    return kFooterBaseline - footer_ascender - fontDescender(font) - kFooterGap;
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

std::int32_t centeredX(std::int32_t width) {
    std::int32_t x = (EPD_WIDTH - width) / 2;
    if (x < 0) {
        x = 0;
    }
    return x;
}

std::int32_t centeredBodyStart(const GFXfont *font, std::size_t line_count) {
    const std::int32_t min_start = bodyStartBaseline(font);
    if (line_count == 0) {
        return min_start;
    }
    const std::int32_t span =
        static_cast<std::int32_t>(line_count - 1) * lineHeight(font);
    const std::int32_t max_last = bodyMaxBaseline(font);
    if (max_last < min_start + span) {
        return min_start;
    }
    return min_start + (max_last - min_start - span) / 2;
}

void drawCenteredQuote(const GFXfont *font, const char *text, std::uint8_t *framebuffer) {
    if (!text || text[0] == '\0') {
        return;
    }

    const std::vector<std::string> lines = wrapLines(font, text, kContentWidth);
    if (lines.empty()) {
        return;
    }

    const std::int32_t lh = lineHeight(font);
    const std::int32_t max_y = bodyMaxBaseline(font);
    const std::int32_t start_y = centeredBodyStart(font, lines.size());

    std::int32_t line_y = start_y;
    for (std::size_t i = 0; i < lines.size(); ++i, line_y += lh) {
        const bool last_slot = line_y + lh > max_y;
        const bool truncated = last_slot && i + 1 < lines.size();
        std::string line = lines[i];
        if (truncated) {
            line += "...";
        }

        const std::int32_t width = measureTextWidth(font, line.c_str());
        const std::int32_t x = centeredX(width);

        if (i == 0) {
            const std::int32_t mark_width = measureTextWidth(font, kOpenQuote);
            std::int32_t mark_x = x - mark_width - kQuoteMarkGap;
            if (mark_x < 8) {
                mark_x = 8;
            }
            drawText(font, kOpenQuote, mark_x, line_y, framebuffer);
        }

        drawText(font, line.c_str(), x, line_y, framebuffer);

        if (truncated || i + 1 == lines.size()) {
            const std::int32_t mark_width = measureTextWidth(font, kCloseQuote);
            std::int32_t mark_x = x + width + kQuoteMarkGap;
            if (mark_x + mark_width > EPD_WIDTH - 8) {
                mark_x = EPD_WIDTH - 8 - mark_width;
            }
            drawText(font, kCloseQuote, mark_x, line_y, framebuffer);
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

void QuoteDisplay::requestFullScrub() {
    full_scrub_next_ = true;
}

void QuoteDisplay::runRepairSequence(const Rect_t &area) {
    // Same waveform as examples/repair/main.cpp. A few short cycles cannot
    // reverse pixels that have been sitting with stacked quotes.
    Serial.println("display: full scrub (~40 s) — keep USB connected");
    epd_clear();
    for (int32_t i = 0; i < 20; i++) {
        epd_push_pixels(area, 50, 0);
        delay(500);
        if ((i + 1) % 5 == 0) {
            Serial.printf("display: black %d/20\n", i + 1);
        }
    }
    epd_clear();
    for (int32_t i = 0; i < 40; i++) {
        epd_push_pixels(area, 50, 1);
        delay(500);
        if ((i + 1) % 10 == 0) {
            Serial.printf("display: white %d/40\n", i + 1);
        }
    }
    epd_clear();
}

void QuoteDisplay::present() {
    const Rect_t area = epd_full_screen();
    const bool full_scrub = full_scrub_next_;
    full_scrub_next_ = false;

    epd_poweron();
    delay(50);

    if (full_scrub) {
        runRepairSequence(area);
    } else {
        epd_clear();
        delay(50);
        epd_draw_image(area, framebuffer_, WHITE_ON_WHITE);
    }

    epd_draw_image(area, framebuffer_, BLACK_ON_WHITE);
    epd_poweroff_all();
}

void QuoteDisplay::show(const QuoteRecord &quote) {
    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    const GFXfont *quote_font = fontForQuote(quote.quote.c_str());
    drawCenteredQuote(quote_font, quote.quote.c_str(), framebuffer_);

    if (!quote.source.empty()) {
        char attribution[160];
        std::snprintf(attribution, sizeof(attribution), "\xE2\x80\x94 %s", quote.source.c_str());
        drawText(kSourceFont, attribution, kMarginX, kFooterBaseline, framebuffer_);
    }

    if (!quote.topic.empty()) {
        const std::int32_t topic_width = measureTextWidth(kTopicFont, quote.topic.c_str());
        const std::int32_t topic_x = EPD_WIDTH - kMarginX - topic_width;
        drawText(kTopicFont, quote.topic.c_str(), topic_x, kFooterBaseline, framebuffer_);
    }

    Serial.printf("quote face advance_y=%u\n", static_cast<unsigned>(quote_font->advance_y));
    present();
}

void QuoteDisplay::showMessage(const char *text) {
    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    if (text != nullptr && text[0] != '\0') {
        std::int32_t cursor_x = 80;
        std::int32_t cursor_y = 260;
        write_string(kUiFont, text, &cursor_x, &cursor_y, framebuffer_);
    }
    present();
}

void QuoteDisplay::powerOff() {
    epd_poweroff_all();
}
