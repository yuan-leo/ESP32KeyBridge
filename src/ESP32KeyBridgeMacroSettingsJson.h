#pragma once

#include <stddef.h>

#include "ESP32KeyBridgeMacroSettings.h"

namespace esp32keybridge
{

bool decodeKeyboardMacroSettingsJson(const char *json, size_t length,
                                     KeyboardMacroSettings &settings,
                                     MacroSettingsError *error = nullptr);

size_t encodeKeyboardMacroSettingsJson(const KeyboardMacroSettings &settings,
                                       char *out, size_t capacity,
                                       MacroSettingsError *error = nullptr);

} // namespace esp32keybridge
