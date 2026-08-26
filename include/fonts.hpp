#pragma once

#include "lora.h"

#include <cstddef>

inline const GFXfont *kUiFont = &Lora28;
inline const GFXfont *kSourceFont = &Lora16;
inline const GFXfont *kTopicFont = &Lora18;

// Largest first. Quote body uses the biggest face that still fits the panel.
inline const GFXfont *kQuoteFonts[] = {&Lora48, &Lora36, &Lora28, &Lora22, &Lora18, &Lora16};
inline constexpr std::size_t kQuoteFontCount = sizeof(kQuoteFonts) / sizeof(kQuoteFonts[0]);

inline const GFXfont *kDisplayFont = kUiFont;
