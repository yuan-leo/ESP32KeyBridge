#include "ESP32KeyBridgeMacroSettings.h"

#include <string.h>

namespace esp32keybridge
{
namespace
{
constexpr uint8_t kMagic[4] = {'E', 'K', 'B', 'M'};
constexpr size_t kHeaderSize = 16;
constexpr size_t kChecksumOffset =
    kHeaderSize + KeyboardMacroSettings::SlotCount *
                      KeyboardMacroSettingsCodec::SlotEncodedSize;

void clearError(MacroSettingsError *error)
{
  if (error != nullptr)
  {
    *error = MacroSettingsError();
  }
}

bool fail(MacroSettingsError *error, MacroSettingsErrorCode code,
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

bool validTrigger(uint16_t usage)
{
  const uint16_t f1 = static_cast<uint16_t>(KeyboardUsage::F1);
  const uint16_t f12 = static_cast<uint16_t>(KeyboardUsage::F12);
  const uint16_t f13 = static_cast<uint16_t>(KeyboardUsage::F13);
  const uint16_t f24 = static_cast<uint16_t>(KeyboardUsage::F24);
  return usage == 0 || (usage >= f1 && usage <= f12) ||
         (usage >= f13 && usage <= f24);
}

bool textLength(const char *text, size_t &bytes, size_t &codepoints,
                MacroSettingsError *error)
{
  bytes = 0;
  codepoints = 0;
  if (text == nullptr)
  {
    return fail(error, MacroSettingsErrorCode::InvalidText,
                "text", "macro text is missing");
  }

  while (text[bytes] != '\0')
  {
    if (bytes >= KeyboardMacroSettings::MaxTextBytes)
    {
      return fail(error, MacroSettingsErrorCode::TextTooLong,
                  "text", "macro text exceeds 124 UTF-8 bytes");
    }
    const uint8_t first = static_cast<uint8_t>(text[bytes]);
    size_t length = 0;
    if (first < 0x80)
    {
      if ((first < 0x20 && first != '\t' && first != '\n' && first != '\r') ||
          first == 0x7f)
      {
        return fail(error, MacroSettingsErrorCode::InvalidText,
                    "text", "macro text contains a control character");
      }
      length = 1;
    }
    else if (first >= 0xc2 && first <= 0xdf)
    {
      length = 2;
    }
    else if (first >= 0xe0 && first <= 0xef)
    {
      length = 3;
    }
    else if (first >= 0xf0 && first <= 0xf4)
    {
      length = 4;
    }
    else
    {
      return fail(error, MacroSettingsErrorCode::InvalidText,
                  "text", "macro text is not valid UTF-8");
    }

    if (bytes + length > KeyboardMacroSettings::MaxTextBytes)
    {
      return fail(error, MacroSettingsErrorCode::TextTooLong,
                  "text", "macro text exceeds 124 UTF-8 bytes");
    }
    for (size_t i = 1; i < length; ++i)
    {
      const uint8_t byte = static_cast<uint8_t>(text[bytes + i]);
      if (byte == 0 || (byte & 0xc0u) != 0x80u)
      {
        return fail(error, MacroSettingsErrorCode::InvalidText,
                    "text", "macro text is not valid UTF-8");
      }
    }
    if ((first == 0xe0 && static_cast<uint8_t>(text[bytes + 1]) < 0xa0) ||
        (first == 0xed && static_cast<uint8_t>(text[bytes + 1]) >= 0xa0) ||
        (first == 0xf0 && static_cast<uint8_t>(text[bytes + 1]) < 0x90) ||
        (first == 0xf4 && static_cast<uint8_t>(text[bytes + 1]) >= 0x90))
    {
      return fail(error, MacroSettingsErrorCode::InvalidText,
                  "text", "macro text is not canonical UTF-8");
    }
    bytes += length;
    if (++codepoints > TextMacro::MaxLength)
    {
      return fail(error, MacroSettingsErrorCode::TextTooLong,
                  "text", "macro text exceeds 31 characters");
    }
  }
  return true;
}

struct SlotRecord
{
  SettingsBlobReadStatus readStatus = SettingsBlobReadStatus::NotFound;
  MacroSettingsDecodeStatus decodeStatus = MacroSettingsDecodeStatus::Invalid;
  KeyboardMacroSettings settings;
  uint32_t generation = 0;
  MacroSettingsError error;

  bool valid() const
  {
    return readStatus == SettingsBlobReadStatus::Read &&
           decodeStatus == MacroSettingsDecodeStatus::Loaded;
  }
};

SlotRecord readSlot(MacroSettingsBlobStorage &storage, uint8_t slot)
{
  SlotRecord record;
  uint8_t bytes[KeyboardMacroSettingsCodec::EncodedSize] = {};
  size_t size = 0;
  record.readStatus = storage.read(slot, bytes, sizeof(bytes), size);
  if (record.readStatus == SettingsBlobReadStatus::Read)
  {
    record.decodeStatus = KeyboardMacroSettingsCodec::decode(
        bytes, size, record.settings, record.generation, &record.error);
  }
  else if (record.readStatus == SettingsBlobReadStatus::Error)
  {
    fail(&record.error, MacroSettingsErrorCode::StorageReadFailed,
         "storage", "could not read the macro settings record");
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
  return slots[1].valid() ? 1 : 0xff;
}
} // namespace

void KeyboardMacroSettings::reset()
{
  for (size_t i = 0; i < SlotCount; ++i)
  {
    slots_[i] = Slot();
  }
}

uint16_t KeyboardMacroSettings::triggerUsage(size_t slot) const
{
  return slot < SlotCount ? slots_[slot].triggerUsage : 0;
}

const char *KeyboardMacroSettings::text(size_t slot) const
{
  return slot < SlotCount ? slots_[slot].text : "";
}

bool KeyboardMacroSettings::usesTrigger(uint16_t triggerUsage) const
{
  if (triggerUsage == 0)
  {
    return false;
  }
  for (size_t i = 0; i < SlotCount; ++i)
  {
    if (slots_[i].triggerUsage == triggerUsage)
    {
      return true;
    }
  }
  return false;
}

bool KeyboardMacroSettings::setSlot(size_t slot, uint16_t triggerUsage,
                                    const char *utf8,
                                    MacroSettingsError *error)
{
  clearError(error);
  if (slot >= SlotCount)
  {
    return fail(error, MacroSettingsErrorCode::InvalidSlot,
                "slot", "macro slot is out of range");
  }
  if (!validTrigger(triggerUsage))
  {
    return fail(error, MacroSettingsErrorCode::InvalidTrigger,
                "triggerUsage", "macro trigger must be F1 through F24 or zero");
  }
  size_t bytes = 0;
  size_t codepoints = 0;
  if (!textLength(utf8, bytes, codepoints, error))
  {
    return false;
  }
  if ((triggerUsage == 0) != (bytes == 0))
  {
    return fail(error, MacroSettingsErrorCode::InvalidText, "text",
                "disabled slots must be empty and enabled slots must contain text");
  }

  KeyboardMacroSettings candidate = *this;
  candidate.slots_[slot] = Slot();
  candidate.slots_[slot].triggerUsage = triggerUsage;
  memcpy(candidate.slots_[slot].text, utf8, bytes);
  candidate.slots_[slot].text[bytes] = '\0';
  if (!candidate.validate(error))
  {
    return false;
  }
  *this = candidate;
  return true;
}

bool KeyboardMacroSettings::validate(MacroSettingsError *error) const
{
  clearError(error);
  for (size_t i = 0; i < SlotCount; ++i)
  {
    if (!validTrigger(slots_[i].triggerUsage))
    {
      return fail(error, MacroSettingsErrorCode::InvalidTrigger,
                  "triggerUsage", "macro trigger must be F1 through F24 or zero");
    }
    size_t bytes = 0;
    size_t codepoints = 0;
    if (!textLength(slots_[i].text, bytes, codepoints, error))
    {
      return false;
    }
    if ((slots_[i].triggerUsage == 0) != (bytes == 0))
    {
      return fail(error, MacroSettingsErrorCode::InvalidText,
                  "slots", "disabled slots must be empty and enabled slots must contain text");
    }
    if (slots_[i].triggerUsage != 0)
    {
      for (size_t j = 0; j < i; ++j)
      {
        if (slots_[j].triggerUsage == slots_[i].triggerUsage)
        {
          return fail(error, MacroSettingsErrorCode::DuplicateTrigger,
                      "triggerUsage", "macro triggers must be unique");
        }
      }
    }
  }
  return true;
}

bool KeyboardMacroSettings::apply(ESP32KeyBridge &bridge,
                                  ESP32KeyBridgeConfig &config,
                                  MacroSettingsError *error) const
{
  if (!validate(error))
  {
    return false;
  }
  config.clearTextMacros();
  for (size_t i = 0; i < SlotCount; ++i)
  {
    if (slots_[i].triggerUsage != 0 &&
        !config.textMacro(keyboardKey(slots_[i].triggerUsage), slots_[i].text))
    {
      return fail(error, MacroSettingsErrorCode::InvalidText,
                  "text", "macro could not be applied to the bridge configuration");
    }
  }
  bridge.applyConfig(config);
  clearError(error);
  return true;
}

bool KeyboardMacroSettings::operator==(const KeyboardMacroSettings &other) const
{
  for (size_t i = 0; i < SlotCount; ++i)
  {
    if (slots_[i].triggerUsage != other.slots_[i].triggerUsage ||
        strcmp(slots_[i].text, other.slots_[i].text) != 0)
    {
      return false;
    }
  }
  return true;
}

bool KeyboardMacroSettingsCodec::encode(const KeyboardMacroSettings &settings,
                                        uint32_t generation, uint8_t *out,
                                        size_t outSize,
                                        MacroSettingsError *error)
{
  clearError(error);
  if (out == nullptr || outSize != EncodedSize)
  {
    return fail(error, MacroSettingsErrorCode::InvalidRecordSize,
                "record", "macro settings record buffer has the wrong size");
  }
  if (!settings.validate(error))
  {
    return false;
  }
  if (generation == 0)
  {
    return fail(error, MacroSettingsErrorCode::InvalidRecordValue,
                "generation", "macro settings generation must not be zero");
  }

  memset(out, 0, EncodedSize);
  memcpy(out, kMagic, sizeof(kMagic));
  out[4] = RecordVersion;
  write16(out, 6, static_cast<uint16_t>(EncodedSize));
  write32(out, 8, generation);
  write32(out, 12, KeyboardMacroSettings::CurrentVersion);
  for (size_t i = 0; i < KeyboardMacroSettings::SlotCount; ++i)
  {
    const size_t offset = kHeaderSize + i * SlotEncodedSize;
    write16(out, offset, settings.slots_[i].triggerUsage);
    const size_t length = strlen(settings.slots_[i].text);
    out[offset + 2] = static_cast<uint8_t>(length);
    memcpy(out + offset + 4, settings.slots_[i].text, length);
  }
  write32(out, kChecksumOffset, crc32(out, kChecksumOffset));
  return true;
}

MacroSettingsDecodeStatus KeyboardMacroSettingsCodec::decode(
    const uint8_t *data, size_t size, KeyboardMacroSettings &settings,
    uint32_t &generation, MacroSettingsError *error)
{
  clearError(error);
  if (data == nullptr || size != EncodedSize)
  {
    fail(error, MacroSettingsErrorCode::InvalidRecordSize,
         "record", "macro settings record has the wrong size");
    return MacroSettingsDecodeStatus::Invalid;
  }
  if (memcmp(data, kMagic, sizeof(kMagic)) != 0)
  {
    fail(error, MacroSettingsErrorCode::InvalidRecordMagic,
         "record", "macro settings record magic does not match");
    return MacroSettingsDecodeStatus::Invalid;
  }
  if (data[4] != RecordVersion)
  {
    fail(error, MacroSettingsErrorCode::UnsupportedRecordVersion,
         "recordVersion", "macro settings record version is not supported");
    return MacroSettingsDecodeStatus::UnsupportedVersion;
  }
  if (data[5] != 0 || read16(data, 6) != EncodedSize)
  {
    fail(error, MacroSettingsErrorCode::InvalidRecordValue,
         "record", "macro settings record header is invalid");
    return MacroSettingsDecodeStatus::Invalid;
  }
  if (read32(data, 12) != KeyboardMacroSettings::CurrentVersion)
  {
    fail(error, MacroSettingsErrorCode::UnsupportedSettingsVersion,
         "settingsVersion", "macro settings schema is not supported");
    return MacroSettingsDecodeStatus::UnsupportedVersion;
  }
  if (read32(data, kChecksumOffset) != crc32(data, kChecksumOffset))
  {
    fail(error, MacroSettingsErrorCode::ChecksumMismatch,
         "checksum", "macro settings record checksum does not match");
    return MacroSettingsDecodeStatus::Invalid;
  }
  const uint32_t decodedGeneration = read32(data, 8);
  if (decodedGeneration == 0)
  {
    fail(error, MacroSettingsErrorCode::InvalidRecordValue,
         "generation", "macro settings generation is invalid");
    return MacroSettingsDecodeStatus::Invalid;
  }

  KeyboardMacroSettings decoded;
  for (size_t i = 0; i < KeyboardMacroSettings::SlotCount; ++i)
  {
    const size_t offset = kHeaderSize + i * SlotEncodedSize;
    const size_t length = data[offset + 2];
    if (data[offset + 3] != 0 || length > KeyboardMacroSettings::MaxTextBytes)
    {
      fail(error, MacroSettingsErrorCode::InvalidRecordValue,
           "record", "macro settings slot metadata is invalid");
      return MacroSettingsDecodeStatus::Invalid;
    }
    for (size_t j = length; j < KeyboardMacroSettings::MaxTextBytes; ++j)
    {
      if (data[offset + 4 + j] != 0)
      {
        fail(error, MacroSettingsErrorCode::InvalidRecordValue,
             "record", "macro settings text padding is not canonical");
        return MacroSettingsDecodeStatus::Invalid;
      }
    }
    char text[KeyboardMacroSettings::MaxTextBytes + 1] = {};
    memcpy(text, data + offset + 4, length);
    if (!decoded.setSlot(i, read16(data, offset), text, error))
    {
      return MacroSettingsDecodeStatus::Invalid;
    }
  }

  settings = decoded;
  generation = decodedGeneration;
  return MacroSettingsDecodeStatus::Loaded;
}

MacroSettingsLoadStatus KeyboardMacroSettingsStore::loadOrDefault(
    KeyboardMacroSettings &settings, MacroSettingsError *error)
{
  clearError(error);
  const SlotRecord slots[2] = {readSlot(storage_, 0), readSlot(storage_, 1)};
  const uint8_t newest = newestValidSlot(slots);
  if (newest != 0xff)
  {
    settings = slots[newest].settings;
    const uint8_t other = static_cast<uint8_t>(newest ^ 1u);
    if ((slots[other].readStatus == SettingsBlobReadStatus::Read &&
         !slots[other].valid()) ||
        slots[other].readStatus == SettingsBlobReadStatus::Error)
    {
      if (error != nullptr)
      {
        *error = slots[other].error;
      }
      return MacroSettingsLoadStatus::LoadedRecovered;
    }
    return MacroSettingsLoadStatus::Loaded;
  }

  settings.reset();
  if (slots[0].readStatus == SettingsBlobReadStatus::Error ||
      slots[1].readStatus == SettingsBlobReadStatus::Error)
  {
    const uint8_t failed =
        slots[0].readStatus == SettingsBlobReadStatus::Error ? 0 : 1;
    if (error != nullptr)
    {
      *error = slots[failed].error;
    }
    return MacroSettingsLoadStatus::DefaultsStorageError;
  }
  if (slots[0].readStatus == SettingsBlobReadStatus::NotFound &&
      slots[1].readStatus == SettingsBlobReadStatus::NotFound)
  {
    return MacroSettingsLoadStatus::DefaultsNoSavedSettings;
  }
  const uint8_t invalid =
      slots[0].readStatus == SettingsBlobReadStatus::Read ? 0 : 1;
  if (error != nullptr)
  {
    *error = slots[invalid].error;
  }
  return MacroSettingsLoadStatus::DefaultsInvalidSettings;
}

bool KeyboardMacroSettingsStore::save(const KeyboardMacroSettings &settings,
                                      MacroSettingsError *error)
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
    const uint8_t failed =
        slots[0].readStatus == SettingsBlobReadStatus::Error ? 0 : 1;
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

  uint8_t bytes[KeyboardMacroSettingsCodec::EncodedSize] = {};
  if (!KeyboardMacroSettingsCodec::encode(settings, generation, bytes,
                                          sizeof(bytes), error))
  {
    return false;
  }
  if (!storage_.write(target, bytes, sizeof(bytes)))
  {
    return fail(error, MacroSettingsErrorCode::StorageWriteFailed,
                "storage", "could not write the macro settings record");
  }
  const SlotRecord verified = readSlot(storage_, target);
  if (!verified.valid() || verified.generation != generation ||
      verified.settings != settings)
  {
    return fail(error, MacroSettingsErrorCode::StorageVerifyFailed,
                "storage", "saved macro settings record could not be verified");
  }
  return true;
}

bool KeyboardMacroSettingsStore::clear(MacroSettingsError *error)
{
  clearError(error);
  const bool first = storage_.erase(0);
  const bool second = storage_.erase(1);
  if (!first || !second)
  {
    return fail(error, MacroSettingsErrorCode::StorageWriteFailed,
                "storage", "could not erase saved macro settings");
  }
  return true;
}

} // namespace esp32keybridge
