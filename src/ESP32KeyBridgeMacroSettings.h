#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ESP32KeyBridge.h"
#include "ESP32KeyBridgeSettings.h"

namespace esp32keybridge
{

enum class MacroSettingsErrorCode : uint8_t
{
  None = 0,
  InvalidSlot,
  InvalidTrigger,
  DuplicateTrigger,
  InvalidText,
  TextTooLong,
  InvalidRecordSize,
  InvalidRecordMagic,
  UnsupportedRecordVersion,
  UnsupportedSettingsVersion,
  InvalidRecordValue,
  ChecksumMismatch,
  StorageReadFailed,
  StorageWriteFailed,
  StorageVerifyFailed,
  InvalidJson,
  MissingJsonField,
  DuplicateJsonField,
  UnknownJsonField,
};

struct MacroSettingsError
{
  MacroSettingsErrorCode code = MacroSettingsErrorCode::None;
  const char *field = nullptr;
  const char *message = nullptr;
};

class KeyboardMacroSettings
{
public:
  static constexpr uint32_t CurrentVersion = 1;
  static constexpr size_t SlotCount = ESP32KeyBridgeConfig::MaxTextMacros;
  static constexpr size_t MaxTextBytes = TextMacro::MaxLength * 4;

  KeyboardMacroSettings() { reset(); }

  void reset();
  uint16_t triggerUsage(size_t slot) const;
  const char *text(size_t slot) const;
  bool usesTrigger(uint16_t triggerUsage) const;

  // Usage zero disables a slot and requires empty text. Enabled slots use
  // F1-F24 and contain 1..31 valid UTF-8 codepoints.
  bool setSlot(size_t slot, uint16_t triggerUsage, const char *utf8,
               MacroSettingsError *error = nullptr);
  bool validate(MacroSettingsError *error = nullptr) const;
  bool apply(ESP32KeyBridge &bridge, ESP32KeyBridgeConfig &config,
             MacroSettingsError *error = nullptr) const;

  bool operator==(const KeyboardMacroSettings &other) const;
  bool operator!=(const KeyboardMacroSettings &other) const
  {
    return !(*this == other);
  }

private:
  friend class KeyboardMacroSettingsCodec;

  struct Slot
  {
    uint16_t triggerUsage = 0;
    char text[MaxTextBytes + 1] = {};
  };

  Slot slots_[SlotCount];
};

enum class MacroSettingsDecodeStatus : uint8_t
{
  Loaded,
  Invalid,
  UnsupportedVersion,
};

class KeyboardMacroSettingsCodec
{
public:
  static constexpr uint8_t RecordVersion = 1;
  static constexpr size_t SlotEncodedSize = 128;
  static constexpr size_t EncodedSize =
      16 + KeyboardMacroSettings::SlotCount * SlotEncodedSize + 4;

  static bool encode(const KeyboardMacroSettings &settings, uint32_t generation,
                     uint8_t *out, size_t outSize,
                     MacroSettingsError *error = nullptr);
  static MacroSettingsDecodeStatus decode(const uint8_t *data, size_t size,
                                          KeyboardMacroSettings &settings,
                                          uint32_t &generation,
                                          MacroSettingsError *error = nullptr);
};

class MacroSettingsBlobStorage
{
public:
  virtual ~MacroSettingsBlobStorage() = default;
  virtual SettingsBlobReadStatus read(uint8_t slot, uint8_t *data,
                                      size_t capacity, size_t &size) = 0;
  virtual bool write(uint8_t slot, const uint8_t *data, size_t size) = 0;
  virtual bool erase(uint8_t slot) = 0;
};

enum class MacroSettingsLoadStatus : uint8_t
{
  Loaded,
  LoadedRecovered,
  DefaultsNoSavedSettings,
  DefaultsInvalidSettings,
  DefaultsStorageError,
};

class KeyboardMacroSettingsStore
{
public:
  explicit KeyboardMacroSettingsStore(MacroSettingsBlobStorage &storage)
      : storage_(storage)
  {
  }

  MacroSettingsLoadStatus loadOrDefault(KeyboardMacroSettings &settings,
                                        MacroSettingsError *error = nullptr);
  bool save(const KeyboardMacroSettings &settings,
            MacroSettingsError *error = nullptr);
  bool clear(MacroSettingsError *error = nullptr);

private:
  MacroSettingsBlobStorage &storage_;
};

} // namespace esp32keybridge
