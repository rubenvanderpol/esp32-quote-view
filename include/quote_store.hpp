#pragma once

#include "quote_record.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class QuoteStore {
public:
    bool begin();
    std::size_t count() const { return quote_count_; }
    bool get(std::size_t index, QuoteRecord &out) const;

private:
    bool loadHeader();
    bool readRecord(std::size_t index, QuoteRecord &out) const;
    const char *topicName(std::uint8_t topic_id) const;

    bool ready_ = false;
    std::uint16_t quote_count_ = 0;
    std::uint8_t topic_count_ = 0;
    std::size_t topics_offset_ = 0;
    std::size_t offsets_offset_ = 0;
    std::size_t pool_offset_ = 0;
    std::vector<std::string> topics_;
};
