#include "ESP32KeyBridgeSettings.h"

#include <string.h>

namespace esp32keybridge
{
namespace
{
constexpr uint8_t kMagic[4] = {'E', 'K', 'B', 'S'};
constexpr size_t kMagicOffset = 0;
constexpr size_t kRecordVersionOffset = 4;
constexpr size_t kReservedHeaderOffset = 5;
constexpr size_t kRecordSizeOffset = 6;
constexpr size_t kGenerationOffset = 8;
constexpr size_t kSettingsVersionOffset = 12;
constexpr size_t kInputLayoutOffset = 16;
constexpr size_t kHostLayoutOffset = 32;
constexpr size_t kToggleUsageOffset = 48;
constexpr size_t kConversionEnabledOffset = 50;
constexpr size_t kReservedPayloadOffset = 51;
constexpr size_t kChecksumOffset = 52;

void clearError(LayoutSettingsError *error)
{
  if (error != nullptr)
  {
    *error = LayoutSettingsError();
  }
}

bool fail(LayoutSettingsError *error, LayoutSettingsErrorCode code,
          const char *field, const char *message)
{
  if (error != nullptr)
  {
    error->code = code;
    error->field = field;
    error->message = message;
  }
  return false;
}

void copyName(char *destination, const char *source)
{
  size_t i = 0;
  while (source[i] != '\0' && i < KeyboardLayoutSettings::MaxLayoutNameLength)
  {
    destination[i] = source[i];
    ++i;
  }
  destination[i] = '\0';
  for (++i; i <= KeyboardLayoutSettings::MaxLayoutNameLength; ++i)
  {
    destination[i] = '\0';
  }
}

bool validLayoutName(const char *name)
{
  if (name == nullptr || name[0] == '\0')
  {
    return false;
  }
  size_t length = 0;
  while (name[length] != '\0')
  {
    if (++length > KeyboardLayoutSettings::MaxLayoutNameLength)
    {
      return false;
    }
  }
  bool found = false;
  KeyboardLayout::byName(name, &found);
  return found;
}

bool validToggleUsage(uint16_t usage)
{
  // Zero disables the toggle. Otherwise accept only named keyboard usages
  // currently exposed by KeyboardUsage, not reserved HID-page values.
  return usage == 0 || (usage >= 0x04 && usage <= 0x73) ||
         (usage >= 0x87 && usage <= 0x98) ||
         (usage >= 0xe0 && usage <= 0xe7);
}

void write16(uint8_t *out, size_t offset, uint16_t value)
{
  out[offset] = static_cast<uint8_t>(value & 0xffu);
  out[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
}

void write32(uint8_t *out, size_t offset, uint32_t value)
{
  out[offset] = static_cast<uint8_t>(value & 0xffu);
  out[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffu);
  out[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffu);
  out[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffu);
}

uint16_t read16(const uint8_t *data, size_t offset)
{
  return static_cast<uint16_t>(data[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t read32(const uint8_t *data, size_t offset)
{
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
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

bool generationIsNewer(uint32_t candidate, uint32_t reference)
{
  return static_cast<int32_t>(candidate - reference) > 0;
}

bool encodedNameIsCanonical(const uint8_t *data, size_t offset)
{
  bool sawTerminator = false;
  for (size_t i = 0; i <= KeyboardLayoutSettings::MaxLayoutNameLength; ++i)
  {
    if (data[offset + i] == 0)
    {
      sawTerminator = true;
    }
    else if (sawTerminator)
    {
      return false;
    }
  }
  return sawTerminator;
}

struct SlotRecord
{
  SettingsBlobReadStatus readStatus = SettingsBlobReadStatus::NotFound;
  LayoutSettingsDecodeStatus decodeStatus = LayoutSettingsDecodeStatus::Invalid;
  KeyboardLayoutSettings settings;
  uint32_t generation = 0;
  LayoutSettingsError error;

  bool valid() const
  {
    return readStatus == SettingsBlobReadStatus::Read &&
           decodeStatus == LayoutSettingsDecodeStatus::Loaded;
  }
};

SlotRecord readSlot(LayoutSettingsBlobStorage &storage, uint8_t slot)
{
  SlotRecord record;
  uint8_t bytes[KeyboardLayoutSettingsCodec::EncodedSize] = {};
  size_t size = 0;
  record.readStatus = storage.read(slot, bytes, sizeof(bytes), size);
  if (record.readStatus == SettingsBlobReadStatus::Read)
  {
    record.decodeStatus = KeyboardLayoutSettingsCodec::decode(
        bytes, size, record.settings, record.generation, &record.error);
  }
  else if (record.readStatus == SettingsBlobReadStatus::Error)
  {
    fail(&record.error, LayoutSettingsErrorCode::StorageReadFailed,
         "storage", "could not read the settings record");
  }
  return record;
}

uint8_t newestValidSlot(const SlotRecord (&slots)[2])
{
  if (slots[0].valid() && slots[1].valid())
  {
    return generationIsNewer(slots[1].generation, slots[0].generation) ? 1 : 0;
  }
  if (slots[0].valid())
  {
    return 0;
  }
  if (slots[1].valid())
  {
    return 1;
  }
  return 0xff;
}
} // namespace

KeyboardLayoutSettings::KeyboardLayoutSettings()
{
  reset();
}

void KeyboardLayoutSettings::reset()
{
  copyName(inputLayoutName_, "en_us");
  copyName(hostLayoutName_, "en_us");
  layoutConversionToggleUsage_ = static_cast<uint16_t>(KeyboardUsage::Pause);
  layoutConversionEnabled_ = false;
}

bool KeyboardLayoutSettings::setInputLayoutName(const char *name,
                                                LayoutSettingsError *error)
{
  clearError(error);
  if (!validLayoutName(name))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidInputLayout,
                "inputLayout", "input layout name is not supported");
  }
  copyName(inputLayoutName_, name);
  return true;
}

bool KeyboardLayoutSettings::setHostLayoutName(const char *name,
                                               LayoutSettingsError *error)
{
  clearError(error);
  if (!validLayoutName(name))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidHostLayout,
                "hostLayout", "host layout name is not supported");
  }
  copyName(hostLayoutName_, name);
  return true;
}

bool KeyboardLayoutSettings::setLayoutConversionToggleUsage(
    uint16_t usage, LayoutSettingsError *error)
{
  clearError(error);
  if (!validToggleUsage(usage))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidToggleUsage,
                "layoutConversionToggleUsage",
                "toggle must be zero or a supported keyboard usage");
  }
  layoutConversionToggleUsage_ = usage;
  return true;
}

bool KeyboardLayoutSettings::validate(LayoutSettingsError *error) const
{
  clearError(error);
  if (!validLayoutName(inputLayoutName_))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidInputLayout,
                "inputLayout", "input layout name is not supported");
  }
  if (!validLayoutName(hostLayoutName_))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidHostLayout,
                "hostLayout", "host layout name is not supported");
  }
  if (!validToggleUsage(layoutConversionToggleUsage_))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidToggleUsage,
                "layoutConversionToggleUsage",
                "toggle must be zero or a supported keyboard usage");
  }
  return true;
}

bool KeyboardLayoutSettings::apply(ESP32KeyBridge &bridge,
                                   ESP32KeyBridgeConfig &config,
                                   InputConfig &inputConfig,
                                   LayoutSettingsError *error) const
{
  if (!validate(error))
  {
    return false;
  }
  bool found = false;
  const KeyboardLayout inputLayout = KeyboardLayout::byName(inputLayoutName_, &found);
  if (!found)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidInputLayout,
                "inputLayout", "input layout name is not supported");
  }
  const KeyboardLayout hostLayout = KeyboardLayout::byName(hostLayoutName_, &found);
  if (!found)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidHostLayout,
                "hostLayout", "host layout name is not supported");
  }

  inputConfig.convertLayout(inputLayout);
  config.hostLayout = hostLayout;
  config.layoutConversionToggle = layoutConversionToggleUsage_ == 0
                                      ? Key()
                                      : keyboardKey(layoutConversionToggleUsage_);
  bridge.applyConfig(config);
  bridge.setLayoutConversionEnabled(layoutConversionEnabled_);
  clearError(error);
  return true;
}

bool KeyboardLayoutSettings::operator==(const KeyboardLayoutSettings &other) const
{
  return strcmp(inputLayoutName_, other.inputLayoutName_) == 0 &&
         strcmp(hostLayoutName_, other.hostLayoutName_) == 0 &&
         layoutConversionToggleUsage_ == other.layoutConversionToggleUsage_ &&
         layoutConversionEnabled_ == other.layoutConversionEnabled_;
}

bool KeyboardLayoutSettingsCodec::encode(const KeyboardLayoutSettings &settings,
                                         uint32_t generation, uint8_t *out,
                                         size_t outSize,
                                         LayoutSettingsError *error)
{
  clearError(error);
  if (out == nullptr || outSize != EncodedSize)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidRecordSize,
                "record", "settings record buffer has the wrong size");
  }
  if (!settings.validate(error))
  {
    return false;
  }
  if (generation == 0)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidRecordValue,
                "generation", "settings generation must not be zero");
  }

  memset(out, 0, EncodedSize);
  memcpy(out + kMagicOffset, kMagic, sizeof(kMagic));
  out[kRecordVersionOffset] = RecordVersion;
  write16(out, kRecordSizeOffset, static_cast<uint16_t>(EncodedSize));
  write32(out, kGenerationOffset, generation);
  write32(out, kSettingsVersionOffset, KeyboardLayoutSettings::CurrentVersion);
  memcpy(out + kInputLayoutOffset, settings.inputLayoutName_,
         sizeof(settings.inputLayoutName_));
  memcpy(out + kHostLayoutOffset, settings.hostLayoutName_,
         sizeof(settings.hostLayoutName_));
  write16(out, kToggleUsageOffset, settings.layoutConversionToggleUsage_);
  out[kConversionEnabledOffset] = settings.layoutConversionEnabled_ ? 1 : 0;
  write32(out, kChecksumOffset, crc32(out, kChecksumOffset));
  return true;
}

LayoutSettingsDecodeStatus KeyboardLayoutSettingsCodec::decode(
    const uint8_t *data, size_t size, KeyboardLayoutSettings &settings,
    uint32_t &generation, LayoutSettingsError *error)
{
  clearError(error);
  if (data == nullptr || size != EncodedSize)
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordSize,
         "record", "settings record has the wrong size");
    return LayoutSettingsDecodeStatus::Invalid;
  }
  if (memcmp(data + kMagicOffset, kMagic, sizeof(kMagic)) != 0)
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordMagic,
         "record", "settings record magic does not match");
    return LayoutSettingsDecodeStatus::Invalid;
  }
  if (data[kRecordVersionOffset] != RecordVersion)
  {
    fail(error, LayoutSettingsErrorCode::UnsupportedRecordVersion,
         "recordVersion", "settings record version is not supported");
    return LayoutSettingsDecodeStatus::UnsupportedVersion;
  }
  if (read16(data, kRecordSizeOffset) != EncodedSize)
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordSize,
         "record", "encoded settings record size does not match");
    return LayoutSettingsDecodeStatus::Invalid;
  }
  if (data[kReservedHeaderOffset] != 0 || data[kReservedPayloadOffset] != 0)
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordReservedValue,
         "record", "reserved settings record fields are not zero");
    return LayoutSettingsDecodeStatus::Invalid;
  }
  if (read32(data, kSettingsVersionOffset) != KeyboardLayoutSettings::CurrentVersion)
  {
    fail(error, LayoutSettingsErrorCode::UnsupportedSettingsVersion,
         "settingsVersion", "stored settings schema is not supported");
    return LayoutSettingsDecodeStatus::UnsupportedVersion;
  }
  if (read32(data, kChecksumOffset) != crc32(data, kChecksumOffset))
  {
    fail(error, LayoutSettingsErrorCode::ChecksumMismatch,
         "checksum", "settings record checksum does not match");
    return LayoutSettingsDecodeStatus::Invalid;
  }

  const uint32_t decodedGeneration = read32(data, kGenerationOffset);
  if (decodedGeneration == 0 || data[kConversionEnabledOffset] > 1 ||
      !encodedNameIsCanonical(data, kInputLayoutOffset) ||
      !encodedNameIsCanonical(data, kHostLayoutOffset))
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordValue,
         "record", "settings record contains an invalid value");
    return LayoutSettingsDecodeStatus::Invalid;
  }

  KeyboardLayoutSettings decoded;
  char inputName[KeyboardLayoutSettings::MaxLayoutNameLength + 1] = {};
  char hostName[KeyboardLayoutSettings::MaxLayoutNameLength + 1] = {};
  memcpy(inputName, data + kInputLayoutOffset, sizeof(inputName));
  memcpy(hostName, data + kHostLayoutOffset, sizeof(hostName));
  if (!decoded.setInputLayoutName(inputName, error) ||
      !decoded.setHostLayoutName(hostName, error) ||
      !decoded.setLayoutConversionToggleUsage(read16(data, kToggleUsageOffset), error))
  {
    return LayoutSettingsDecodeStatus::Invalid;
  }
  decoded.setLayoutConversionEnabled(data[kConversionEnabledOffset] != 0);
  if (!decoded.validate(error))
  {
    return LayoutSettingsDecodeStatus::Invalid;
  }

  settings = decoded;
  generation = decodedGeneration;
  return LayoutSettingsDecodeStatus::Loaded;
}

LayoutSettingsLoadStatus KeyboardLayoutSettingsStore::loadOrDefault(
    KeyboardLayoutSettings &settings, LayoutSettingsError *error)
{
  clearError(error);
  const SlotRecord slots[2] = {readSlot(storage_, 0), readSlot(storage_, 1)};
  const uint8_t newest = newestValidSlot(slots);
  if (newest != 0xff)
  {
    settings = slots[newest].settings;
    const uint8_t other = static_cast<uint8_t>(newest ^ 1u);
    if (slots[other].readStatus == SettingsBlobReadStatus::Read && !slots[other].valid())
    {
      if (error != nullptr)
      {
        *error = slots[other].error;
      }
      return LayoutSettingsLoadStatus::LoadedRecovered;
    }
    if (slots[other].readStatus == SettingsBlobReadStatus::Error)
    {
      if (error != nullptr)
      {
        *error = slots[other].error;
      }
      return LayoutSettingsLoadStatus::LoadedRecovered;
    }
    return LayoutSettingsLoadStatus::Loaded;
  }

  settings.reset();
  if (slots[0].readStatus == SettingsBlobReadStatus::Error ||
      slots[1].readStatus == SettingsBlobReadStatus::Error)
  {
    const uint8_t failed = slots[0].readStatus == SettingsBlobReadStatus::Error ? 0 : 1;
    if (error != nullptr)
    {
      *error = slots[failed].error;
    }
    return LayoutSettingsLoadStatus::DefaultsStorageError;
  }
  if (slots[0].readStatus == SettingsBlobReadStatus::NotFound &&
      slots[1].readStatus == SettingsBlobReadStatus::NotFound)
  {
    return LayoutSettingsLoadStatus::DefaultsNoSavedSettings;
  }

  const uint8_t invalid = slots[0].readStatus == SettingsBlobReadStatus::Read ? 0 : 1;
  if (error != nullptr)
  {
    *error = slots[invalid].error;
  }
  return LayoutSettingsLoadStatus::DefaultsInvalidSettings;
}

bool KeyboardLayoutSettingsStore::save(const KeyboardLayoutSettings &settings,
                                       LayoutSettingsError *error)
{
  clearError(error);
  if (!settings.validate(error))
  {
    return false;
  }

  const SlotRecord slots[2] = {readSlot(storage_, 0), readSlot(storage_, 1)};
  if (slots[0].readStatus == SettingsBlobReadStatus::Error ||
      slots[1].readStatus == SettingsBlobReadStatus::Error)
  {
    const uint8_t failed = slots[0].readStatus == SettingsBlobReadStatus::Error ? 0 : 1;
    if (error != nullptr)
    {
      *error = slots[failed].error;
    }
    return false;
  }
  const uint8_t newest = newestValidSlot(slots);
  const uint8_t target = newest == 0xff ? 0 : static_cast<uint8_t>(newest ^ 1u);
  uint32_t generation = newest == 0xff ? 1 : slots[newest].generation + 1;
  if (generation == 0)
  {
    generation = 1;
  }

  uint8_t bytes[KeyboardLayoutSettingsCodec::EncodedSize] = {};
  if (!KeyboardLayoutSettingsCodec::encode(settings, generation, bytes,
                                           sizeof(bytes), error))
  {
    return false;
  }
  if (!storage_.write(target, bytes, sizeof(bytes)))
  {
    return fail(error, LayoutSettingsErrorCode::StorageWriteFailed,
                "storage", "could not write the settings record");
  }

  const SlotRecord verified = readSlot(storage_, target);
  if (!verified.valid() || verified.generation != generation ||
      verified.settings != settings)
  {
    return fail(error, LayoutSettingsErrorCode::StorageVerifyFailed,
                "storage", "saved settings record could not be verified");
  }
  return true;
}

bool KeyboardLayoutSettingsStore::clear(LayoutSettingsError *error)
{
  clearError(error);
  const bool first = storage_.erase(0);
  const bool second = storage_.erase(1);
  if (!first || !second)
  {
    return fail(error, LayoutSettingsErrorCode::StorageWriteFailed,
                "storage", "could not erase saved settings");
  }
  return true;
}

} // namespace esp32keybridge
