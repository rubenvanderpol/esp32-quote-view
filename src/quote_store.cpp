#include "quote_store.hpp"

#include <Arduino.h>
#include <LittleFS.h>

#include <cstring>
#include <string>

#include "quote_format.hpp"

bool QuoteStore::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return false;
    }

    if (!loadHeader()) {
        Serial.println("quotes.bin missing or invalid — run tools/pack_quotes and uploadfs");
        return false;
    }

    ready_ = true;
    Serial.printf("Loaded %u quotes, %u topics\n", quote_count_, topic_count_);
    return true;
}

bool QuoteStore::loadHeader() {
    File file = LittleFS.open(quote::kQuotesPath, "r");
    if (!file) {
        return false;
    }

    char magic[4] = {};
    if (file.read(reinterpret_cast<std::uint8_t *>(magic), 4) != 4
        || std::memcmp(magic, quote::kMagic, 4) != 0) {
        file.close();
        return false;
    }

    const std::uint8_t version = file.read();
    if (version != quote::kFormatVersion) {
        file.close();
        return false;
    }

    std::uint8_t count_bytes[3] = {};
    if (file.read(count_bytes, 3) != 3) {
        file.close();
        return false;
    }

    quote_count_ = static_cast<std::uint16_t>(count_bytes[0] | (count_bytes[1] << 8));
    topic_count_ = count_bytes[2];

    if (file.read() != 0) {
        file.close();
        return false;
    }

    topics_offset_ = quote::kHeaderSize;
    topics_.clear();
    topics_.reserve(topic_count_);

    file.seek(topics_offset_);
    for (std::uint8_t i = 0; i < topic_count_; ++i) {
        std::string topic;
        while (file.available()) {
            const int ch = file.read();
            if (ch < 0) {
                file.close();
                return false;
            }
            if (ch == 0) {
                break;
            }
            topic.push_back(static_cast<char>(ch));
        }
        topics_.push_back(std::move(topic));
    }

    offsets_offset_ = file.position();
    pool_offset_ = offsets_offset_ + (static_cast<std::size_t>(quote_count_) * sizeof(std::uint32_t));
    file.close();
    return quote_count_ > 0;
}

const char *QuoteStore::topicName(std::uint8_t topic_id) const {
    if (topic_id >= topics_.size()) {
        return "unknown";
    }
    return topics_[topic_id].c_str();
}

bool QuoteStore::readRecord(std::size_t index, QuoteRecord &out) const {
    if (!ready_ || index >= quote_count_) {
        return false;
    }

    File file = LittleFS.open(quote::kQuotesPath, "r");
    if (!file) {
        return false;
    }

    file.seek(offsets_offset_ + (index * sizeof(std::uint32_t)));
    std::uint8_t offset_bytes[4] = {};
    if (file.read(offset_bytes, 4) != 4) {
        file.close();
        return false;
    }

    const std::uint32_t record_offset = offset_bytes[0]
        | (offset_bytes[1] << 8)
        | (offset_bytes[2] << 16)
        | (offset_bytes[3] << 24);

    file.seek(pool_offset_ + record_offset);

    const int topic_id = file.read();
    if (topic_id < 0) {
        file.close();
        return false;
    }

    std::uint8_t lengths[3] = {};
    if (file.read(lengths, 3) != 3) {
        file.close();
        return false;
    }

    const std::uint16_t quote_len = static_cast<std::uint16_t>(lengths[0] | (lengths[1] << 8));
    const std::uint8_t source_len = lengths[2];

    out.quote.clear();
    out.quote.reserve(quote_len);
    for (std::uint16_t i = 0; i < quote_len; ++i) {
        const int ch = file.read();
        if (ch < 0) {
            file.close();
            return false;
        }
        out.quote.push_back(static_cast<char>(ch));
    }

    out.source.clear();
    out.source.reserve(source_len);
    for (std::uint8_t i = 0; i < source_len; ++i) {
        const int ch = file.read();
        if (ch < 0) {
            file.close();
            return false;
        }
        out.source.push_back(static_cast<char>(ch));
    }

    out.topic = topicName(static_cast<std::uint8_t>(topic_id));
    file.close();
    return true;
}

bool QuoteStore::get(std::size_t index, QuoteRecord &out) const {
    return readRecord(index, out);
}
