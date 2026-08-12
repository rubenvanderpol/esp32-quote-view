#include "quote_store.h"

#include <LittleFS.h>

namespace {

constexpr char kQuotesPath[] = "/quotes.bin";
constexpr char kMagic[] = "QTE1";
constexpr uint8_t kVersion = 1;

}  // namespace

bool QuoteStore::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return false;
    }

    if (!loadHeader()) {
        Serial.println("quotes.bin missing or invalid — run pack_quotes.py and uploadfs");
        return false;
    }

    ready_ = true;
    Serial.printf("Loaded %u quotes, %u topics\n", quote_count_, topic_count_);
    return true;
}

bool QuoteStore::loadHeader() {
    File file = LittleFS.open(kQuotesPath, "r");
    if (!file) {
        return false;
    }

    char magic[4] = {};
    if (file.read((uint8_t *)magic, 4) != 4 || memcmp(magic, kMagic, 4) != 0) {
        file.close();
        return false;
    }

    uint8_t version = file.read();
    if (version != kVersion) {
        file.close();
        return false;
    }

    uint8_t count_bytes[3] = {};
    if (file.read(count_bytes, 3) != 3) {
        file.close();
        return false;
    }

    quote_count_ = count_bytes[0] | (count_bytes[1] << 8);
    topic_count_ = count_bytes[2];

    if (file.read() != 0) {  // reserved byte
        file.close();
        return false;
    }

    topics_offset_ = 9;
    topics_.clear();
    topics_.reserve(topic_count_);

    file.seek(topics_offset_);
    for (uint8_t i = 0; i < topic_count_; ++i) {
        String topic;
        while (file.available()) {
            int ch = file.read();
            if (ch < 0) {
                file.close();
                return false;
            }
            if (ch == 0) {
                break;
            }
            topic += static_cast<char>(ch);
        }
        topics_.push_back(topic);
    }

    offsets_offset_ = file.position();
    pool_offset_ = offsets_offset_ + (static_cast<size_t>(quote_count_) * sizeof(uint32_t));
    file.close();
    return quote_count_ > 0;
}

const char *QuoteStore::topicName(uint8_t topic_id) const {
    if (topic_id >= topics_.size()) {
        return "unknown";
    }
    return topics_[topic_id].c_str();
}

bool QuoteStore::readRecord(size_t index, QuoteRecord &out) const {
    if (!ready_ || index >= quote_count_) {
        return false;
    }

    File file = LittleFS.open(kQuotesPath, "r");
    if (!file) {
        return false;
    }

    file.seek(offsets_offset_ + (index * sizeof(uint32_t)));
    uint8_t offset_bytes[4] = {};
    if (file.read(offset_bytes, 4) != 4) {
        file.close();
        return false;
    }

    uint32_t record_offset = offset_bytes[0]
        | (offset_bytes[1] << 8)
        | (offset_bytes[2] << 16)
        | (offset_bytes[3] << 24);

    file.seek(pool_offset_ + record_offset);

    int topic_id = file.read();
    if (topic_id < 0) {
        file.close();
        return false;
    }

    uint8_t lengths[3] = {};
    if (file.read(lengths, 3) != 3) {
        file.close();
        return false;
    }

    uint16_t quote_len = lengths[0] | (lengths[1] << 8);
    uint8_t source_len = lengths[2];

    out.quote.reserve(quote_len);
    out.quote = "";
    for (uint16_t i = 0; i < quote_len; ++i) {
        int ch = file.read();
        if (ch < 0) {
            file.close();
            return false;
        }
        out.quote += static_cast<char>(ch);
    }

    out.source.reserve(source_len);
    out.source = "";
    for (uint8_t i = 0; i < source_len; ++i) {
        int ch = file.read();
        if (ch < 0) {
            file.close();
            return false;
        }
        out.source += static_cast<char>(ch);
    }

    out.topic = topicName(static_cast<uint8_t>(topic_id));
    file.close();
    return true;
}

bool QuoteStore::get(size_t index, QuoteRecord &out) const {
    return readRecord(index, out);
}
