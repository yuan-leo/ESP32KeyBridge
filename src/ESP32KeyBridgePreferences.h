#pragma once

#if !defined(ARDUINO_ARCH_ESP32)
#error "ESP32KeyBridgePreferences requires the Arduino ESP32 core"
#endif

#include <Preferences.h>

#include "ESP32KeyBridgeMacroSettings.h"
#include "ESP32KeyBridgeSettings.h"

namespace esp32keybridge
{

// NVS backend for KeyboardLayoutSettingsStore. It uses one namespace and two
// alternating blobs; the store owns validation, CRC checking, and recovery.
class PreferencesLayoutSettingsStorage : public LayoutSettingsBlobStorage
{
public:
  static constexpr const char *Namespace = "ekb";

  SettingsBlobReadStatus read(uint8_t slot, uint8_t *data, size_t capacity,
                              size_t &size) override
  {
    size = 0;
    if (slot > 1 || data == nullptr)
    {
      return SettingsBlobReadStatus::Error;
    }
    Preferences preferences;
    // Read-write mode creates the namespace on first boot. Preferences cannot
    // distinguish a missing read-only namespace from an NVS open failure.
    if (!preferences.begin(Namespace, false))
    {
      return SettingsBlobReadStatus::Error;
    }
    const char *key = keyFor(slot);
    const size_t storedSize = preferences.getBytesLength(key);
    if (storedSize == 0)
    {
      preferences.end();
      return SettingsBlobReadStatus::NotFound;
    }
    if (storedSize > capacity)
    {
      size = storedSize;
      preferences.end();
      return SettingsBlobReadStatus::Read;
    }
    const size_t readSize = preferences.getBytes(key, data, storedSize);
    preferences.end();
    size = readSize;
    return readSize == storedSize ? SettingsBlobReadStatus::Read
                                  : SettingsBlobReadStatus::Error;
  }

  bool write(uint8_t slot, const uint8_t *data, size_t size) override
  {
    if (slot > 1 || data == nullptr || size == 0)
    {
      return false;
    }
    Preferences preferences;
    if (!preferences.begin(Namespace, false))
    {
      return false;
    }
    const bool written = preferences.putBytes(keyFor(slot), data, size) == size;
    preferences.end();
    return written;
  }

  bool erase(uint8_t slot) override
  {
    if (slot > 1)
    {
      return false;
    }
    Preferences preferences;
    if (!preferences.begin(Namespace, false))
    {
      return false;
    }
    const char *key = keyFor(slot);
    const bool erased = !preferences.isKey(key) || preferences.remove(key);
    preferences.end();
    return erased;
  }

private:
  static const char *keyFor(uint8_t slot)
  {
    return slot == 0 ? "layout_a" : "layout_b";
  }
};

// Separate alternating records keep macro evolution and recovery independent
// from the smaller layout settings record.
class PreferencesMacroSettingsStorage : public MacroSettingsBlobStorage
{
public:
  SettingsBlobReadStatus read(uint8_t slot, uint8_t *data, size_t capacity,
                              size_t &size) override
  {
    size = 0;
    if (slot > 1 || data == nullptr)
    {
      return SettingsBlobReadStatus::Error;
    }
    Preferences preferences;
    if (!preferences.begin(PreferencesLayoutSettingsStorage::Namespace, false))
    {
      return SettingsBlobReadStatus::Error;
    }
    const char *key = keyFor(slot);
    const size_t storedSize = preferences.getBytesLength(key);
    if (storedSize == 0)
    {
      preferences.end();
      return SettingsBlobReadStatus::NotFound;
    }
    if (storedSize > capacity)
    {
      size = storedSize;
      preferences.end();
      return SettingsBlobReadStatus::Read;
    }
    const size_t readSize = preferences.getBytes(key, data, storedSize);
    preferences.end();
    size = readSize;
    return readSize == storedSize ? SettingsBlobReadStatus::Read
                                  : SettingsBlobReadStatus::Error;
  }

  bool write(uint8_t slot, const uint8_t *data, size_t size) override
  {
    if (slot > 1 || data == nullptr || size == 0)
    {
      return false;
    }
    Preferences preferences;
    if (!preferences.begin(PreferencesLayoutSettingsStorage::Namespace, false))
    {
      return false;
    }
    const bool written = preferences.putBytes(keyFor(slot), data, size) == size;
    preferences.end();
    return written;
  }

  bool erase(uint8_t slot) override
  {
    if (slot > 1)
    {
      return false;
    }
    Preferences preferences;
    if (!preferences.begin(PreferencesLayoutSettingsStorage::Namespace, false))
    {
      return false;
    }
    const char *key = keyFor(slot);
    const bool erased = !preferences.isKey(key) || preferences.remove(key);
    preferences.end();
    return erased;
  }

private:
  static const char *keyFor(uint8_t slot)
  {
    return slot == 0 ? "macro_a" : "macro_b";
  }
};

} // namespace esp32keybridge
