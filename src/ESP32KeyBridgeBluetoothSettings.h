#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ESP32KeyBridgeBluetooth.h"
#include "ESP32KeyBridgeSettings.h"

namespace esp32keybridge
{

struct BluetoothProfileRecord
{
  char address[18] = {};
  char label[BluetoothProfileSnapshot::MaxLabelBytes + 1] = {};
  char hostName[BluetoothProfileSnapshot::MaxHostNameBytes + 1] = {};
  uint8_t addressType = 0;
  bool occupied = false;
};

class BluetoothProfileRegistry
{
public:
  static constexpr size_t ProfileCount = 3;

  void reset();
  size_t selectedSlot() const { return selectedSlot_; }
  const BluetoothProfileRecord &profile(size_t slot) const;
  bool select(size_t slot);
  bool assign(size_t slot, const char *address, uint8_t addressType);
  bool clear(size_t slot);
  bool setLabel(size_t slot, const char *label);
  bool setHostName(size_t slot, const char *hostName);
  int findAddress(const char *address, uint8_t addressType) const;
  bool operator==(const BluetoothProfileRegistry &other) const;
  bool operator!=(const BluetoothProfileRegistry &other) const { return !(*this == other); }

private:
  friend class BluetoothProfileSettingsCodec;
  BluetoothProfileRecord profiles_[ProfileCount];
  size_t selectedSlot_ = 1;
};

enum class BluetoothProfileSettingsDecodeStatus : uint8_t
{
  Loaded,
  Invalid,
  UnsupportedVersion,
};

class BluetoothProfileSettingsCodec
{
public:
  static constexpr uint8_t RecordVersion = 2;
  static constexpr size_t EncodedSize = 352;
  static bool encode(const BluetoothProfileRegistry &registry, uint32_t generation,
                     uint8_t *out, size_t capacity);
  static BluetoothProfileSettingsDecodeStatus decode(const uint8_t *data, size_t size,
                                                       BluetoothProfileRegistry &registry,
                                                       uint32_t &generation);
};

class BluetoothProfileBlobStorage
{
public:
  virtual ~BluetoothProfileBlobStorage() = default;
  virtual SettingsBlobReadStatus read(uint8_t slot, uint8_t *data, size_t capacity,
                                      size_t &size) = 0;
  virtual bool write(uint8_t slot, const uint8_t *data, size_t size) = 0;
  virtual bool erase(uint8_t slot) = 0;
};

enum class BluetoothProfileSettingsLoadStatus : uint8_t
{
  Loaded,
  LoadedRecovered,
  DefaultsNoSavedSettings,
  DefaultsInvalidSettings,
  DefaultsStorageError,
};

class BluetoothProfileSettingsStore
{
public:
  explicit BluetoothProfileSettingsStore(BluetoothProfileBlobStorage &storage)
      : storage_(storage) {}
  BluetoothProfileSettingsLoadStatus loadOrDefault(BluetoothProfileRegistry &registry);
  bool save(const BluetoothProfileRegistry &registry);
  bool clear();

private:
  BluetoothProfileBlobStorage &storage_;
};

} // namespace esp32keybridge
