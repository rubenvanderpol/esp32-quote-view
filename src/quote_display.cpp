#include "quote_display.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "epd_driver.h"
#include "firasans.h"
#include "utilities.h"

namespace {

constexpr std::int32_t kMarginX = 48;
constexpr std::int32_t kContentWidth = EPD_WIDTH - (kMarginX * 2);
constexpr std::int32_t kBodyLineHeight = 52;

void drawCentered(const char *text, std::int32_t y, std::uint8_t *framebuffer) {
    std::int32_t cursor_x = kMarginX;
    std::int32_t cursor_y = y;
    write_string((GFXfont *)&FiraSans, const_cast<char *>(text), &cursor_x, &cursor_y, framebuffer);
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

std::int32_t QuoteDisplay::measureLineWidth(const char *start, const char *end) {
    char buffer[256];
    std::size_t len = static_cast<std::size_t>(end - start);
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    std::memcpy(buffer, start, len);
    buffer[len] = '\0';

    std::int32_t cursor_x = 0;
    std::int32_t cursor_y = 0;
    write_string((GFXfont *)&FiraSans, buffer, &cursor_x, &cursor_y, nullptr);
    return cursor_x;
}

void QuoteDisplay::drawWrappedText(const char *text, std::int32_t x, std::int32_t y,
                                   std::int32_t max_width, std::int32_t line_height,
                                   std::uint8_t *framebuffer) {
    if (!text || text[0] == '\0') {
        return;
    }

    const char *word = text;
    const char *cursor = text;
    std::int32_t line_y = y;

    while (*cursor) {
        while (*cursor && *cursor != ' ' && *cursor != '\n') {
            ++cursor;
        }

        const std::int32_t candidate_width = measureLineWidth(text, cursor);
        if (candidate_width > max_width && word > text) {
            char line[256];
            std::size_t line_len = static_cast<std::size_t>(word - text - 1);
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1;
            }
            std::memcpy(line, text, line_len);
            line[line_len] = '\0';

            std::int32_t cursor_x = x;
            std::int32_t cursor_y = line_y;
            write_string((GFXfont *)&FiraSans, line, &cursor_x, &cursor_y, framebuffer);
            line_y += line_height;

            while (*word == ' ') {
                ++word;
            }
            text = word;
            cursor = word;
            continue;
        }

        if (*cursor == '\n') {
            char line[256];
            std::size_t line_len = static_cast<std::size_t>(cursor - text);
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1;
            }
            std::memcpy(line, text, line_len);
            line[line_len] = '\0';

            std::int32_t cursor_x = x;
            std::int32_t cursor_y = line_y;
            write_string((GFXfont *)&FiraSans, line, &cursor_x, &cursor_y, framebuffer);
            line_y += line_height;

            text = cursor + 1;
            word = text;
            ++cursor;
            continue;
        }

        if (*cursor == '\0') {
            break;
        }

        word = cursor;
        ++cursor;
        while (*cursor == ' ') {
            ++cursor;
        }
        word = cursor;
    }

    if (text < cursor || *text) {
        std::int32_t cursor_x = x;
        std::int32_t cursor_y = line_y;
        write_string((GFXfont *)&FiraSans, const_cast<char *>(text), &cursor_x, &cursor_y, framebuffer);
    }
}

void QuoteDisplay::show(const QuoteRecord &quote, std::size_t index, std::size_t total,
                        std::uint32_t seconds_until_next) {
    std::memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    epd_fill_rect(kMarginX, 36, 220, 52, 0x0000, framebuffer_);
    FontProperties inverted = {
        .fg_color = 15,
        .bg_color = 0,
        .fallback_glyph = 0,
        .flags = 0,
    };
    std::int32_t topic_x = kMarginX + 16;
    std::int32_t topic_y = 72;
    write_mode((GFXfont *)&FiraSans, const_cast<char *>(quote.topic.c_str()), &topic_x,
               &topic_y, framebuffer_, WHITE_ON_BLACK, &inverted);

    std::int32_t mark_x = kMarginX;
    std::int32_t mark_y = 150;
    write_string((GFXfont *)&FiraSans, const_cast<char *>("\xE2\x80\x9C"), &mark_x, &mark_y, framebuffer_);

    drawWrappedText(quote.quote.c_str(), kMarginX, 210, kContentWidth, kBodyLineHeight, framebuffer_);

    char attribution[160];
    std::snprintf(attribution, sizeof(attribution), "\xE2\x80\x94 %s", quote.source.c_str());
    std::int32_t attr_x = kMarginX;
    std::int32_t attr_y = EPD_HEIGHT - 150;
    write_string((GFXfont *)&FiraSans, attribution, &attr_x, &attr_y, framebuffer_);

    char footer[80];
    if (seconds_until_next > 0) {
        const std::uint32_t hours = seconds_until_next / 3600U;
        const std::uint32_t minutes = (seconds_until_next % 3600U) / 60U;
        if (hours > 0) {
            std::snprintf(footer, sizeof(footer), "%u / %u   Next in %uh %um",
                          static_cast<unsigned>(index + 1), static_cast<unsigned>(total), hours, minutes);
        } else {
            std::snprintf(footer, sizeof(footer), "%u / %u   Next in %um",
                          static_cast<unsigned>(index + 1), static_cast<unsigned>(total), minutes);
        }
    } else {
        std::snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(index + 1),
                      static_cast<unsigned>(total));
    }
    drawCentered(footer, EPD_HEIGHT - 70, framebuffer_);

    epd_draw_hline(kMarginX, EPD_HEIGHT - 110, kContentWidth, 0, framebuffer_);

    epd_poweron();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
    epd_poweroff_all();
}

void QuoteDisplay::powerOff() {
    epd_poweroff_all();
}
