#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/quote_format.hpp"

namespace {

struct QuoteEntry {
    std::string quote;
    std::string topic;
    std::string source;
};

struct Document {
    std::vector<std::string> topics;
    std::vector<QuoteEntry> quotes;
};

void appendU16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

class JsonReader {
public:
    explicit JsonReader(std::string input) : input_(std::move(input)) {}

    Document parseDocument() {
        skipWhitespace();
        expect('{');
        Document doc;

        while (true) {
            skipWhitespace();
            if (peek() == '}') {
                ++index_;
                break;
            }

            const std::string key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();

            if (key == "topics") {
                doc.topics = parseStringArray();
            } else if (key == "quotes") {
                doc.quotes = parseQuoteArray();
            } else {
                skipValue();
            }

            skipWhitespace();
            if (peek() == ',') {
                ++index_;
                continue;
            }
        }

        return doc;
    }

private:
    char peek() const {
        return input_[index_];
    }

    void expect(char ch) {
        skipWhitespace();
        if (input_[index_] != ch) {
            throw std::runtime_error(std::string("Expected '") + ch + "'");
        }
        ++index_;
    }

    void skipWhitespace() {
        while (index_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[index_]))) {
            ++index_;
        }
    }

    std::string parseString() {
        skipWhitespace();
        expect('"');
        std::string value;
        while (index_ < input_.size() && input_[index_] != '"') {
            if (input_[index_] == '\\') {
                ++index_;
                if (index_ >= input_.size()) {
                    break;
                }
                value.push_back(input_[index_++]);
                continue;
            }
            value.push_back(input_[index_++]);
        }
        expect('"');
        return value;
    }

    void skipValue() {
        skipWhitespace();
        const char ch = peek();
        if (ch == '"') {
            parseString();
            return;
        }
        if (ch == '{') {
            ++index_;
            while (peek() != '}') {
                parseString();
                expect(':');
                skipValue();
                skipWhitespace();
                if (peek() == ',') {
                    ++index_;
                }
            }
            expect('}');
            return;
        }
        if (ch == '[') {
            ++index_;
            while (peek() != ']') {
                skipValue();
                skipWhitespace();
                if (peek() == ',') {
                    ++index_;
                }
            }
            expect(']');
            return;
        }
        while (index_ < input_.size() && input_[index_] != ',' && input_[index_] != '}' && input_[index_] != ']') {
            ++index_;
        }
    }

    std::vector<std::string> parseStringArray() {
        std::vector<std::string> values;
        skipWhitespace();
        expect('[');
        skipWhitespace();
        if (peek() == ']') {
            ++index_;
            return values;
        }

        while (true) {
            values.push_back(parseString());
            skipWhitespace();
            if (peek() == ']') {
                ++index_;
                break;
            }
            expect(',');
        }
        return values;
    }

    std::vector<QuoteEntry> parseQuoteArray() {
        std::vector<QuoteEntry> values;
        skipWhitespace();
        expect('[');
        skipWhitespace();
        if (peek() == ']') {
            ++index_;
            return values;
        }

        while (true) {
            skipWhitespace();
            expect('{');
            QuoteEntry entry;
            while (true) {
                skipWhitespace();
                if (peek() == '}') {
                    ++index_;
                    break;
                }
                const std::string key = parseString();
                skipWhitespace();
                expect(':');
                skipWhitespace();
                const std::string value = parseString();
                if (key == "quote") {
                    entry.quote = value;
                } else if (key == "topic") {
                    entry.topic = value;
                } else if (key == "source") {
                    entry.source = value;
                }
                skipWhitespace();
                if (peek() == ',') {
                    ++index_;
                }
            }
            values.push_back(std::move(entry));
            skipWhitespace();
            if (peek() == ']') {
                ++index_;
                break;
            }
            expect(',');
        }
        return values;
    }

    std::string input_;
    std::size_t index_ = 0;
};

std::vector<std::uint8_t> packDocument(const Document &doc) {
    if (doc.quotes.empty()) {
        throw std::runtime_error("quotes array is empty");
    }
    if (doc.topics.empty()) {
        throw std::runtime_error("topics array is empty");
    }
    if (doc.quotes.size() > 0xFFFF) {
        throw std::runtime_error("too many quotes");
    }

    std::unordered_map<std::string, std::uint8_t> topic_index;
    for (std::size_t i = 0; i < doc.topics.size(); ++i) {
        topic_index.emplace(doc.topics[i], static_cast<std::uint8_t>(i));
    }

    std::vector<std::uint8_t> topics_blob;
    for (const auto &topic : doc.topics) {
        topics_blob.insert(topics_blob.end(), topic.begin(), topic.end());
        topics_blob.push_back(0);
    }

    std::vector<std::uint8_t> pool;
    std::vector<std::uint32_t> offsets;

    for (const auto &entry : doc.quotes) {
        const auto it = topic_index.find(entry.topic);
        if (it == topic_index.end()) {
            throw std::runtime_error("unknown topic: " + entry.topic);
        }
        if (entry.quote.size() > 0xFFFF) {
            throw std::runtime_error("quote too long");
        }
        if (entry.source.size() > 0xFF) {
            throw std::runtime_error("source too long");
        }

        offsets.push_back(static_cast<std::uint32_t>(pool.size()));
        pool.push_back(it->second);
        appendU16(pool, static_cast<std::uint16_t>(entry.quote.size()));
        pool.push_back(static_cast<std::uint8_t>(entry.source.size()));
        pool.insert(pool.end(), entry.quote.begin(), entry.quote.end());
        pool.insert(pool.end(), entry.source.begin(), entry.source.end());
    }

    std::vector<std::uint8_t> blob;
    blob.insert(blob.end(), quote::kMagic, quote::kMagic + 4);
    blob.push_back(quote::kFormatVersion);
    appendU16(blob, static_cast<std::uint16_t>(doc.quotes.size()));
    blob.push_back(static_cast<std::uint8_t>(doc.topics.size()));
    blob.push_back(0);
    blob.insert(blob.end(), topics_blob.begin(), topics_blob.end());
    for (const auto offset : offsets) {
        appendU32(blob, offset);
    }
    blob.insert(blob.end(), pool.begin(), pool.end());
    return blob;
}

}  // namespace

int main(int argc, char **argv) {
    const char *input_path = "data/quotes.json";
    const char *output_path = "data/quotes.bin";
    if (argc > 1) {
        input_path = argv[1];
    }
    if (argc > 2) {
        output_path = argv[2];
    }

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open " << input_path << '\n';
        return 1;
    }

    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  try {
        const Document doc = JsonReader(json).parseDocument();
        const std::vector<std::uint8_t> blob = packDocument(doc);

        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cerr << "Failed to open " << output_path << '\n';
            return 1;
        }
        output.write(reinterpret_cast<const char *>(blob.data()),
                       static_cast<std::streamsize>(blob.size()));
        std::cout << "Packed " << doc.quotes.size() << " quotes (" << blob.size() << " bytes) -> "
                  << output_path << '\n';
    } catch (const std::exception &ex) {
        std::cerr << "pack_quotes failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
