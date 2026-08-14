#pragma once

#include <cstddef>
#include <cstdint>

namespace quote {

inline constexpr char kMagic[4] = {'Q', 'T', 'E', '1'};
inline constexpr uint8_t kFormatVersion = 1;
inline constexpr std::size_t kHeaderSize = 9;
inline constexpr char kQuotesPath[] = "/quotes.bin";

// Longest quote the 960x540 layout can render (with truncation headroom).
// Enforced by tools/pack_quotes so oversized quotes fail at build time.
inline constexpr std::size_t kMaxQuoteBytes = 400;

}  // namespace quote
