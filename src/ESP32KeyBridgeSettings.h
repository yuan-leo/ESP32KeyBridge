#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ESP32KeyBridge.h"

namespace esp32keybridge
{

enum class LayoutSettingsErrorCode : uint8_t
{
  None = 0,
  InvalidInputLayout,
  InvalidHostLayout,
  InvalidToggleUsage,
  InvalidRecordSize,
  InvalidRecordMagic,
  UnsupportedRecordVersion,
  UnsupportedSettingsVersion,
  InvalidRecordReservedValue,
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

struct LayoutSettingsError
{
  LayoutSettingsErrorCode code = LayoutSettingsErrorCode::None;
  const char *field = nullptr;
  const char *message = nullptr;
};

// The layout portion of the future web configuration. Layout names are
// persisted instead of KeyboardLayout's internal pointers, so records remain
// valid across firmware builds. Phase 1 intentionally models one keyboard
// input, matching the ESP32-S3's single active USB role.
class KeyboardLayoutSettings
{
public:
  static constexpr uint32_t CurrentVersion = 1;
  static constexpr size_t MaxLayoutNameLength = 15;

  KeyboardLayoutSettings();

  void reset();

  const char *inputLayoutName() const { return inputLayoutName_; }
  const char *hostLayoutName() const { return hostLayoutName_; }
  uint16_t layoutConversionToggleUsage() const { return layoutConversionToggleUsage_; }
  bool layoutConversionEnabled() const { return layoutConversionEnabled_; }

  // A setter changes the value only when the complete candidate is valid.
  bool setInputLayoutName(const char *name, LayoutSettingsError *error = nullptr);
  bool setHostLayoutName(const char *name, LayoutSettingsError *error = nullptr);
  bool setLayoutConversionToggleUsage(uint16_t usage,
                                      LayoutSettingsError *error = nullptr);
  void setLayoutConversionEnabled(bool enabled) { layoutConversionEnabled_ = enabled; }

  bool validate(LayoutSettingsError *error = nullptr) const;

  // Applies only layout-related fields and preserves remaps, layers, macros,
  // and other settings already present in config/inputConfig. Call this from
  // the same execution context as bridge.update().
  bool apply(ESP32KeyBridge &bridge, ESP32KeyBridgeConfig &config,
             InputConfig &inputConfig, LayoutSettingsError *error = nullptr) const;

  bool operator==(const KeyboardLayoutSettings &other) const;
  bool operator!=(const KeyboardLayoutSettings &other) const { return !(*this == other); }

private:
  friend class KeyboardLayoutSettingsCodec;

  char inputLayoutName_[MaxLayoutNameLength + 1] = {};
  char hostLayoutName_[MaxLayoutNameLength + 1] = {};
  uint16_t layoutConversionToggleUsage_ = 0;
  bool layoutConversionEnabled_ = false;
};

enum class LayoutSettingsDecodeStatus : uint8_t
{
  Loaded,
  Invalid,
  UnsupportedVersion,
};

// Fixed-size, endian-stable record with a CRC-32. The generation belongs to
// the persistence envelope, not to the user-facing settings.
class KeyboardLayoutSettingsCodec
{
public:
  static constexpr uint8_t RecordVersion = 1;
  static constexpr size_t EncodedSize = 56;

  static bool encode(const KeyboardLayoutSettings &settings, uint32_t generation,
                     uint8_t *out, size_t outSize,
                     LayoutSettingsError *error = nullptr);
  static LayoutSettingsDecodeStatus decode(const uint8_t *data, size_t size,
                                           KeyboardLayoutSettings &settings,
                                           uint32_t &generation,
                                           LayoutSettingsError *error = nullptr);
};

enum class SettingsBlobReadStatus : uint8_t
{
  Read,
  NotFound,
  Error,
};

// Two small blobs are all the persistence manager needs. Implementations can
// use NVS, a file, or an in-memory fake in unit tests.
class LayoutSettingsBlobStorage
{
public:
  virtual ~LayoutSettingsBlobStorage() = default;
  virtual SettingsBlobReadStatus read(uint8_t slot, uint8_t *data, size_t capacity,
                                      size_t &size) = 0;
  virtual bool write(uint8_t slot, const uint8_t *data, size_t size) = 0;
  virtual bool erase(uint8_t slot) = 0;
};

enum class LayoutSettingsLoadStatus : uint8_t
{
  Loaded,
  LoadedRecovered,
  DefaultsNoSavedSettings,
  DefaultsInvalidSettings,
  DefaultsStorageError,
};

// Alternates between two records. A save never overwrites the newest valid
// record, and the just-written record is read back before success is reported.
class KeyboardLayoutSettingsStore
{
public:
  explicit KeyboardLayoutSettingsStore(LayoutSettingsBlobStorage &storage)
      : storage_(storage)
  {
  }

  LayoutSettingsLoadStatus loadOrDefault(KeyboardLayoutSettings &settings,
                                         LayoutSettingsError *error = nullptr);
  bool save(const KeyboardLayoutSettings &settings,
            LayoutSettingsError *error = nullptr);
  bool clear(LayoutSettingsError *error = nullptr);

private:
  LayoutSettingsBlobStorage &storage_;
};

} // namespace esp32keybridge
