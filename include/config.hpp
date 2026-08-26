#pragma once

// How long each quote stays on screen before advancing to the next in quotes.bin.
#ifndef QUOTE_INTERVAL_HOURS
#define QUOTE_INTERVAL_HOURS 6
#endif

// Power down the ESP32-S3 and e-paper between updates (~hundreds of µA on battery).
// Set to 0 while debugging over USB if you want continuous serial output and button polling.
// Even with this on, a USB host connection keeps the chip awake so uploads still work.
#ifndef ENABLE_DEEP_SLEEP
#define ENABLE_DEEP_SLEEP 1
#endif
