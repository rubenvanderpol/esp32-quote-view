#pragma once

#include <Arduino.h>
#include <vector>

struct QuoteRecord {
    String quote;
    String topic;
    String source;
};

class QuoteStore {
public:
    bool begin();
    size_t count() const { return quote_count_; }
    bool get(size_t index, QuoteRecord &out) const;

private:
    bool loadHeader();
    bool readRecord(size_t index, QuoteRecord &out) const;
    const char *topicName(uint8_t topic_id) const;

    bool ready_ = false;
    uint16_t quote_count_ = 0;
    uint8_t topic_count_ = 0;
    size_t topics_offset_ = 0;
    size_t offsets_offset_ = 0;
    size_t pool_offset_ = 0;
    std::vector<String> topics_;
};
