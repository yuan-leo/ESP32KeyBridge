#pragma once

#include <stddef.h>

#include "ESP32KeyBridgeSettings.h"

namespace esp32keybridge
{

// Strict, allocation-free JSON codec for the Phase 3 configuration endpoint.
// The decoder requires every schema field exactly once and rejects unknown
// fields, duplicate fields, escapes, trailing input, and unsupported values.
bool decodeKeyboardLayoutSettingsJson(const char *json, size_t length,
                                      KeyboardLayoutSettings &settings,
                                      LayoutSettingsError *error = nullptr);

// Returns the number of bytes written, excluding the terminator, or zero when
// the output buffer is too small or settings are invalid.
size_t encodeKeyboardLayoutSettingsJson(const KeyboardLayoutSettings &settings,
                                        char *out, size_t capacity,
                                        LayoutSettingsError *error = nullptr);

} // namespace esp32keybridge
