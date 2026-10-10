#include "ESP32KeyBridgeBluetoothSettings.h"

#include <string.h>

namespace esp32keybridge
{
namespace
{
constexpr uint8_t kMagic[4] = {'E', 'K', 'B', 'P'};
constexpr size_t kGeneration = 8;
constexpr size_t kSelected = 12;
constexpr size_t kProfiles = 16;
constexpr size_t kStride = 45;
constexpr size_t kChecksum = BluetoothProfileSettingsCodec::EncodedSize - 4;

void write32(uint8_t *p, size_t o, uint32_t v)
{
  for (size_t i = 0; i < 4; ++i) p[o + i] = static_cast<uint8_t>(v >> (i * 8));
}
uint32_t read32(const uint8_t *p, size_t o)
{
  return static_cast<uint32_t>(p[o]) | (static_cast<uint32_t>(p[o + 1]) << 8) |
         (static_cast<uint32_t>(p[o + 2]) << 16) |
         (static_cast<uint32_t>(p[o + 3]) << 24);
}
uint32_t crc32(const uint8_t *data, size_t size)
{
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < size; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}
bool newer(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }
bool normalizeAddress(const char *source, char (&out)[18])
{
  if (source == nullptr || strlen(source) != 17) return false;
  for (size_t i = 0; i < 17; ++i)
  {
    if (i % 3 == 2)
    {
      if (source[i] != ':') return false;
      out[i] = ':';
      continue;
    }
    char c = source[i];
    if (c >= 'a' && c <= 'f') c = static_cast<char>(c - 'a' + 'A');
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    out[i] = c;
  }
  out[17] = '\0';
  return true;
}
bool validUtf8(const char *text, size_t &length)
{
  if (text == nullptr) return false;
  length = strlen(text);
  if (length > BluetoothProfileSnapshot::MaxLabelBytes) return false;
  for (size_t i = 0; i < length;)
  {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    size_t extra = c < 0x80 ? 0 : (c >= 0xc2 && c <= 0xdf ? 1 :
        (c >= 0xe0 && c <= 0xef ? 2 : (c >= 0xf0 && c <= 0xf4 ? 3 : 99)));
    if (extra == 99 || i + extra >= length) return false;
    for (size_t j = 1; j <= extra; ++j)
      if ((static_cast<uint8_t>(text[i + j]) & 0xc0) != 0x80) return false;
    if (extra == 2)
    {
      const uint8_t second = static_cast<uint8_t>(text[i + 1]);
      if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second >= 0xa0)) return false;
    }
    if (extra == 3)
    {
      const uint8_t second = static_cast<uint8_t>(text[i + 1]);
      if ((c == 0xf0 && second < 0x90) || (c == 0xf4 && second >= 0x90)) return false;
    }
    i += extra + 1;
  }
  return true;
}
struct ReadRecord
{
  SettingsBlobReadStatus read = SettingsBlobReadStatus::NotFound;
  BluetoothProfileSettingsDecodeStatus decoded = BluetoothProfileSettingsDecodeStatus::Invalid;
  BluetoothProfileRegistry registry;
  uint32_t generation = 0;
  bool valid() const { return read == SettingsBlobReadStatus::Read && decoded == BluetoothProfileSettingsDecodeStatus::Loaded; }
};
ReadRecord readRecord(BluetoothProfileBlobStorage &storage, uint8_t slot)
{
  ReadRecord result;
  uint8_t bytes[BluetoothProfileSettingsCodec::EncodedSize] = {};
  size_t size = 0;
  result.read = storage.read(slot, bytes, sizeof(bytes), size);
  if (result.read == SettingsBlobReadStatus::Read)
    result.decoded = BluetoothProfileSettingsCodec::decode(bytes, size, result.registry, result.generation);
  return result;
}
}

void BluetoothProfileRegistry::reset()
{
  memset(profiles_, 0, sizeof(profiles_));
  selectedSlot_ = 1;
}

const BluetoothProfileRecord &BluetoothProfileRegistry::profile(size_t slot) const
{
  static const BluetoothProfileRecord empty;
  return slot >= 1 && slot <= ProfileCount ? profiles_[slot - 1] : empty;
}

bool BluetoothProfileRegistry::select(size_t slot)
{
  if (slot < 1 || slot > ProfileCount) return false;
  selectedSlot_ = slot;
  return true;
}

bool BluetoothProfileRegistry::assign(size_t slot, const char *address, uint8_t addressType)
{
  if (slot < 1 || slot > ProfileCount) return false;
  char normalized[18] = {};
  if (!normalizeAddress(address, normalized)) return false;
  BluetoothProfileRecord next = profiles_[slot - 1];
  memcpy(next.address, normalized, sizeof(next.address));
  next.addressType = addressType;
  next.occupied = true;
  profiles_[slot - 1] = next;
  return true;
}

bool BluetoothProfileRegistry::clear(size_t slot)
{
  if (slot < 1 || slot > ProfileCount) return false;
  char label[BluetoothProfileSnapshot::MaxLabelBytes + 1] = {};
  memcpy(label, profiles_[slot - 1].label, sizeof(label));
  profiles_[slot - 1] = BluetoothProfileRecord();
  memcpy(profiles_[slot - 1].label, label, sizeof(label));
  return true;
}

bool BluetoothProfileRegistry::setLabel(size_t slot, const char *label)
{
  if (slot < 1 || slot > ProfileCount) return false;
  size_t length = 0;
  if (!validUtf8(label, length)) return false;
  memset(profiles_[slot - 1].label, 0, sizeof(profiles_[slot - 1].label));
  memcpy(profiles_[slot - 1].label, label, length);
  return true;
}

int BluetoothProfileRegistry::findAddress(const char *address, uint8_t addressType) const
{
  char normalized[18] = {};
  if (!normalizeAddress(address, normalized)) return -1;
  for (size_t i = 0; i < ProfileCount; ++i)
    if (profiles_[i].occupied && profiles_[i].addressType == addressType &&
        strcmp(profiles_[i].address, normalized) == 0) return static_cast<int>(i + 1);
  return -1;
}

bool BluetoothProfileRegistry::operator==(const BluetoothProfileRegistry &other) const
{
  return selectedSlot_ == other.selectedSlot_ &&
         memcmp(profiles_, other.profiles_, sizeof(profiles_)) == 0;
}

bool BluetoothProfileSettingsCodec::encode(const BluetoothProfileRegistry &registry,
                                            uint32_t generation, uint8_t *out,
                                            size_t capacity)
{
  if (out == nullptr || capacity < EncodedSize) return false;
  memset(out, 0, EncodedSize);
  memcpy(out, kMagic, sizeof(kMagic));
  out[4] = RecordVersion;
  out[6] = static_cast<uint8_t>(EncodedSize & 0xff);
  out[7] = static_cast<uint8_t>(EncodedSize >> 8);
  write32(out, kGeneration, generation);
  out[kSelected] = static_cast<uint8_t>(registry.selectedSlot_);
  for (size_t i = 0; i < BluetoothProfileRegistry::ProfileCount; ++i)
  {
    const size_t o = kProfiles + i * kStride;
    const BluetoothProfileRecord &p = registry.profiles_[i];
    out[o] = p.occupied ? 1 : 0;
    out[o + 1] = p.addressType;
    memcpy(out + o + 2, p.address, sizeof(p.address));
    memcpy(out + o + 20, p.label, sizeof(p.label));
  }
  write32(out, kChecksum, crc32(out, kChecksum));
  return true;
}

BluetoothProfileSettingsDecodeStatus BluetoothProfileSettingsCodec::decode(
    const uint8_t *data, size_t size, BluetoothProfileRegistry &registry,
    uint32_t &generation)
{
  if (data == nullptr || size != EncodedSize || memcmp(data, kMagic, 4) != 0 ||
      data[6] != (EncodedSize & 0xff) || data[7] != (EncodedSize >> 8) ||
      read32(data, kChecksum) != crc32(data, kChecksum))
    return BluetoothProfileSettingsDecodeStatus::Invalid;
  if (data[4] != RecordVersion) return BluetoothProfileSettingsDecodeStatus::UnsupportedVersion;
  BluetoothProfileRegistry next;
  next.reset();
  if (!next.select(data[kSelected])) return BluetoothProfileSettingsDecodeStatus::Invalid;
  for (size_t i = 0; i < BluetoothProfileRegistry::ProfileCount; ++i)
  {
    const size_t o = kProfiles + i * kStride;
    if (data[o] > 1 || data[o + 19] != 0 || data[o + 44] != 0)
      return BluetoothProfileSettingsDecodeStatus::Invalid;
    const char *address = reinterpret_cast<const char *>(data + o + 2);
    const char *label = reinterpret_cast<const char *>(data + o + 20);
    if (!next.setLabel(i + 1, label)) return BluetoothProfileSettingsDecodeStatus::Invalid;
    if (data[o] && !next.assign(i + 1, address, data[o + 1]))
      return BluetoothProfileSettingsDecodeStatus::Invalid;
  }
  registry = next;
  generation = read32(data, kGeneration);
  return BluetoothProfileSettingsDecodeStatus::Loaded;
}

BluetoothProfileSettingsLoadStatus BluetoothProfileSettingsStore::loadOrDefault(
    BluetoothProfileRegistry &registry)
{
  const ReadRecord records[2] = {readRecord(storage_, 0), readRecord(storage_, 1)};
  if (records[0].valid() || records[1].valid())
  {
    const uint8_t best = records[0].valid() && records[1].valid()
        ? (newer(records[1].generation, records[0].generation) ? 1 : 0)
        : (records[1].valid() ? 1 : 0);
    registry = records[best].registry;
    return records[0].valid() && records[1].valid()
        ? BluetoothProfileSettingsLoadStatus::Loaded
        : BluetoothProfileSettingsLoadStatus::LoadedRecovered;
  }
  registry.reset();
  if (records[0].read == SettingsBlobReadStatus::Error || records[1].read == SettingsBlobReadStatus::Error)
    return BluetoothProfileSettingsLoadStatus::DefaultsStorageError;
  if (records[0].read == SettingsBlobReadStatus::NotFound && records[1].read == SettingsBlobReadStatus::NotFound)
    return BluetoothProfileSettingsLoadStatus::DefaultsNoSavedSettings;
  return BluetoothProfileSettingsLoadStatus::DefaultsInvalidSettings;
}

bool BluetoothProfileSettingsStore::save(const BluetoothProfileRegistry &registry)
{
  const ReadRecord records[2] = {readRecord(storage_, 0), readRecord(storage_, 1)};
  uint8_t target = 0;
  uint32_t generation = 1;
  if (records[0].valid() || records[1].valid())
  {
    const uint8_t newest = records[0].valid() && records[1].valid()
        ? (newer(records[1].generation, records[0].generation) ? 1 : 0)
        : (records[1].valid() ? 1 : 0);
    target = static_cast<uint8_t>(1 - newest);
    generation = records[newest].generation + 1;
  }
  uint8_t bytes[BluetoothProfileSettingsCodec::EncodedSize] = {};
  if (!BluetoothProfileSettingsCodec::encode(registry, generation, bytes, sizeof(bytes)) ||
      !storage_.write(target, bytes, sizeof(bytes))) return false;
  const ReadRecord verified = readRecord(storage_, target);
  return verified.valid() && verified.generation == generation && verified.registry == registry;
}

bool BluetoothProfileSettingsStore::clear()
{
  return storage_.erase(0) && storage_.erase(1);
}

} // namespace esp32keybridge
