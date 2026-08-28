#pragma once

#include "quote_record.hpp"

#include <cstdint>

class QuoteDisplay {
public:
    bool begin();
    void show(const QuoteRecord &quote);
    void showMessage(const char *text);
    void powerOff();

private:
    void present();

    std::uint8_t *framebuffer_ = nullptr;
};
