#include "quote_display.h"

#include <string.h>

#include "epd_driver.h"
#include "firasans.h"
#include "utilities.h"

namespace {

constexpr int32_t kMarginX = 48;
constexpr int32_t kMarginY = 56;
constexpr int32_t kContentWidth = EPD_WIDTH - (kMarginX * 2);
constexpr int32_t kBodyLineHeight = 52;
constexpr int32_t kMetaLineHeight = 40;

void drawCentered(const char *text, int32_t y, uint8_t *framebuffer) {
    int32_t cursor_x = kMarginX;
    int32_t cursor_y = y;
    write_string((GFXfont *)&FiraSans, (char *)text, &cursor_x, &cursor_y, framebuffer);
}

}  // namespace

bool QuoteDisplay::begin() {
    epd_init();

    framebuffer_ = static_cast<uint8_t *>(
        ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2));
    if (!framebuffer_) {
        Serial.println("framebuffer alloc failed");
        return false;
    }

    memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    return true;
}

int32_t QuoteDisplay::measureLineWidth(const char *start, const char *end) {
    char buffer[256];
    size_t len = static_cast<size_t>(end - start);
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    memcpy(buffer, start, len);
    buffer[len] = '\0';

    int32_t cursor_x = 0;
    int32_t cursor_y = 0;
    write_string((GFXfont *)&FiraSans, buffer, &cursor_x, &cursor_y, nullptr);
    return cursor_x;
}

void QuoteDisplay::drawWrappedText(const char *text, int32_t x, int32_t y,
                                   int32_t max_width, int32_t line_height,
                                   uint8_t *framebuffer) {
    if (!text || text[0] == '\0') {
        return;
    }

    const char *word = text;
    const char *cursor = text;
    int32_t line_y = y;

    while (*cursor) {
        while (*cursor && *cursor != ' ' && *cursor != '\n') {
            ++cursor;
        }

        int32_t candidate_width = measureLineWidth(text, cursor);
        if (candidate_width > max_width && word > text) {
            char line[256];
            size_t line_len = static_cast<size_t>(word - text - 1);
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1;
            }
            memcpy(line, text, line_len);
            line[line_len] = '\0';

            int32_t cursor_x = x;
            int32_t cursor_y = line_y;
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
            size_t line_len = static_cast<size_t>(cursor - text);
            if (line_len >= sizeof(line)) {
                line_len = sizeof(line) - 1;
            }
            memcpy(line, text, line_len);
            line[line_len] = '\0';

            int32_t cursor_x = x;
            int32_t cursor_y = line_y;
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
        int32_t cursor_x = x;
        int32_t cursor_y = line_y;
        write_string((GFXfont *)&FiraSans, (char *)text, &cursor_x, &cursor_y, framebuffer);
    }
}

void QuoteDisplay::show(const QuoteRecord &quote, size_t index, size_t total) {
    memset(framebuffer_, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  // Topic badge
    epd_fill_rect(kMarginX, 36, 220, 52, 0x0000, framebuffer_);
    FontProperties inverted = {
        .fg_color = 15,
        .bg_color = 0,
        .fallback_glyph = 0,
        .flags = 0,
    };
    int32_t topic_x = kMarginX + 16;
    int32_t topic_y = 72;
    write_mode((GFXfont *)&FiraSans, quote.topic.c_str(), &topic_x, &topic_y,
               framebuffer_, WHITE_ON_BLACK, &inverted);

  // Opening quote mark
    int32_t mark_x = kMarginX;
    int32_t mark_y = 150;
    write_string((GFXfont *)&FiraSans, (char *)"\xE2\x80\x9C", &mark_x, &mark_y, framebuffer_);

  // Quote body
    drawWrappedText(quote.quote.c_str(), kMarginX, 210, kContentWidth, kBodyLineHeight,
                    framebuffer_);

  // Attribution
    char attribution[160];
    snprintf(attribution, sizeof(attribution), "\xE2\x80\x94 %s", quote.source.c_str());
    int32_t attr_x = kMarginX;
    int32_t attr_y = EPD_HEIGHT - 150;
    write_string((GFXfont *)&FiraSans, attribution, &attr_x, &attr_y, framebuffer_);

  // Footer / navigation hint
    char footer[64];
    snprintf(footer, sizeof(footer), "%u / %u   Press button for next", static_cast<unsigned>(index + 1),
             static_cast<unsigned>(total));
    drawCentered(footer, EPD_HEIGHT - 70, framebuffer_);

    epd_draw_hline(kMarginX, EPD_HEIGHT - 110, kContentWidth, 0, framebuffer_);

    epd_poweron();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
    epd_poweroff_all();
}

void QuoteDisplay::powerOff() {
    epd_poweroff_all();
}
