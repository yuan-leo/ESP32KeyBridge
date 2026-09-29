#include "ESP32KeyBridgeMacroSettingsJson.h"

#include <stdio.h>
#include <string.h>

namespace esp32keybridge
{
namespace
{
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

class JsonParser
{
public:
  JsonParser(const char *json, size_t length) : cursor_(json), end_(json + length) {}

  void whitespace()
  {
    while (cursor_ < end_ &&
           (*cursor_ == ' ' || *cursor_ == '\t' ||
            *cursor_ == '\r' || *cursor_ == '\n'))
    {
      ++cursor_;
    }
  }

  bool consume(char expected)
  {
    whitespace();
    if (cursor_ >= end_ || *cursor_ != expected)
    {
      return false;
    }
    ++cursor_;
    return true;
  }

  bool string(char *out, size_t capacity)
  {
    whitespace();
    if (cursor_ >= end_ || *cursor_ != '"' || capacity == 0)
    {
      return false;
    }
    ++cursor_;
    size_t used = 0;
    while (cursor_ < end_ && *cursor_ != '"')
    {
      const uint8_t byte = static_cast<uint8_t>(*cursor_++);
      if (byte < 0x20)
      {
        return false;
      }
      if (byte != '\\')
      {
        if (!append(out, capacity, used, byte))
        {
          return false;
        }
        continue;
      }
      if (cursor_ >= end_)
      {
        return false;
      }
      const char escaped = *cursor_++;
      switch (escaped)
      {
      case '"':
      case '\\':
      case '/':
        if (!append(out, capacity, used, static_cast<uint8_t>(escaped)))
        {
          return false;
        }
        break;
      case 'b':
        if (!append(out, capacity, used, '\b')) return false;
        break;
      case 'f':
        if (!append(out, capacity, used, '\f')) return false;
        break;
      case 'n':
        if (!append(out, capacity, used, '\n')) return false;
        break;
      case 'r':
        if (!append(out, capacity, used, '\r')) return false;
        break;
      case 't':
        if (!append(out, capacity, used, '\t')) return false;
        break;
      case 'u':
      {
        uint16_t first = 0;
        if (!hex4(first))
        {
          return false;
        }
        uint32_t codepoint = first;
        if (first >= 0xd800 && first <= 0xdbff)
        {
          if (end_ - cursor_ < 6 || cursor_[0] != '\\' || cursor_[1] != 'u')
          {
            return false;
          }
          cursor_ += 2;
          uint16_t second = 0;
          if (!hex4(second) || second < 0xdc00 || second > 0xdfff)
          {
            return false;
          }
          codepoint = 0x10000u +
                      ((static_cast<uint32_t>(first) - 0xd800u) << 10) +
                      (static_cast<uint32_t>(second) - 0xdc00u);
        }
        else if (first >= 0xdc00 && first <= 0xdfff)
        {
          return false;
        }
        if (!appendCodepoint(out, capacity, used, codepoint))
        {
          return false;
        }
        break;
      }
      default:
        return false;
      }
    }
    if (cursor_ >= end_ || *cursor_ != '"')
    {
      return false;
    }
    ++cursor_;
    out[used] = '\0';
    return true;
  }

  bool unsignedInteger(uint32_t &value)
  {
    whitespace();
    if (cursor_ >= end_ || *cursor_ < '0' || *cursor_ > '9')
    {
      return false;
    }
    if (*cursor_ == '0' && cursor_ + 1 < end_ &&
        cursor_[1] >= '0' && cursor_[1] <= '9')
    {
      return false;
    }
    uint32_t parsed = 0;
    do
    {
      const uint8_t digit = static_cast<uint8_t>(*cursor_ - '0');
      if (parsed > (0xffffffffu - digit) / 10u)
      {
        return false;
      }
      parsed = parsed * 10u + digit;
      ++cursor_;
    } while (cursor_ < end_ && *cursor_ >= '0' && *cursor_ <= '9');
    value = parsed;
    return true;
  }

  bool done()
  {
    whitespace();
    return cursor_ == end_;
  }

private:
  static bool append(char *out, size_t capacity, size_t &used, uint8_t byte)
  {
    if (byte == 0 || used + 1 >= capacity)
    {
      return false;
    }
    out[used++] = static_cast<char>(byte);
    return true;
  }

  static bool appendCodepoint(char *out, size_t capacity, size_t &used,
                              uint32_t codepoint)
  {
    if (codepoint == 0 || codepoint > 0x10ffffu)
    {
      return false;
    }
    if (codepoint < 0x80)
    {
      return append(out, capacity, used, static_cast<uint8_t>(codepoint));
    }
    if (codepoint < 0x800)
    {
      return append(out, capacity, used,
                    static_cast<uint8_t>(0xc0u | (codepoint >> 6))) &&
             append(out, capacity, used,
                    static_cast<uint8_t>(0x80u | (codepoint & 0x3fu)));
    }
    if (codepoint < 0x10000)
    {
      return append(out, capacity, used,
                    static_cast<uint8_t>(0xe0u | (codepoint >> 12))) &&
             append(out, capacity, used,
                    static_cast<uint8_t>(0x80u | ((codepoint >> 6) & 0x3fu))) &&
             append(out, capacity, used,
                    static_cast<uint8_t>(0x80u | (codepoint & 0x3fu)));
    }
    return append(out, capacity, used,
                  static_cast<uint8_t>(0xf0u | (codepoint >> 18))) &&
           append(out, capacity, used,
                  static_cast<uint8_t>(0x80u | ((codepoint >> 12) & 0x3fu))) &&
           append(out, capacity, used,
                  static_cast<uint8_t>(0x80u | ((codepoint >> 6) & 0x3fu))) &&
           append(out, capacity, used,
                  static_cast<uint8_t>(0x80u | (codepoint & 0x3fu)));
  }

  static int hexValue(char value)
  {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  }

  bool hex4(uint16_t &value)
  {
    if (end_ - cursor_ < 4)
    {
      return false;
    }
    uint16_t parsed = 0;
    for (size_t i = 0; i < 4; ++i)
    {
      const int digit = hexValue(cursor_[i]);
      if (digit < 0)
      {
        return false;
      }
      parsed = static_cast<uint16_t>((parsed << 4) | digit);
    }
    cursor_ += 4;
    value = parsed;
    return true;
  }

  const char *cursor_;
  const char *end_;
};

bool markField(uint8_t &mask, uint8_t field, MacroSettingsError *error,
               const char *name)
{
  if ((mask & field) != 0)
  {
    return fail(error, MacroSettingsErrorCode::DuplicateJsonField,
                name, "JSON field appears more than once");
  }
  mask |= field;
  return true;
}

bool parseSlot(JsonParser &parser, size_t index,
               KeyboardMacroSettings &settings, MacroSettingsError *error)
{
  if (!parser.consume('{'))
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "slots", "macro slot must be a JSON object");
  }
  uint8_t fields = 0;
  uint32_t trigger = 0;
  char text[KeyboardMacroSettings::MaxTextBytes + 1] = {};
  bool afterComma = false;
  for (;;)
  {
    if (parser.consume('}'))
    {
      if (afterComma)
      {
        return fail(error, MacroSettingsErrorCode::InvalidJson,
                    "slots", "macro slot must not end with a trailing comma");
      }
      break;
    }
    afterComma = false;
    char key[24] = {};
    if (!parser.string(key, sizeof(key)) || !parser.consume(':'))
    {
      return fail(error, MacroSettingsErrorCode::InvalidJson,
                  "slots", "macro slot contains invalid syntax");
    }
    if (strcmp(key, "triggerUsage") == 0)
    {
      if (!markField(fields, 1u, error, "triggerUsage") ||
          !parser.unsignedInteger(trigger))
      {
        return error != nullptr && error->code != MacroSettingsErrorCode::None
                   ? false
                   : fail(error, MacroSettingsErrorCode::InvalidJson,
                          "triggerUsage", "triggerUsage must be an unsigned integer");
      }
    }
    else if (strcmp(key, "text") == 0)
    {
      if (!markField(fields, 2u, error, "text") ||
          !parser.string(text, sizeof(text)))
      {
        return error != nullptr && error->code != MacroSettingsErrorCode::None
                   ? false
                   : fail(error, MacroSettingsErrorCode::InvalidJson,
                          "text", "text must be a JSON string within the macro limit");
      }
    }
    else
    {
      return fail(error, MacroSettingsErrorCode::UnknownJsonField,
                  "slots", "macro slot contains an unknown field");
    }
    if (parser.consume('}'))
    {
      break;
    }
    if (!parser.consume(','))
    {
      return fail(error, MacroSettingsErrorCode::InvalidJson,
                  "slots", "macro slot fields must be comma-separated");
    }
    afterComma = true;
  }
  if (fields != 3u)
  {
    return fail(error, MacroSettingsErrorCode::MissingJsonField,
                "slots", "macro slot is missing a required field");
  }
  if (trigger > 0xffffu)
  {
    return fail(error, MacroSettingsErrorCode::InvalidTrigger,
                "triggerUsage", "macro trigger is out of range");
  }
  return settings.setSlot(index, static_cast<uint16_t>(trigger), text, error);
}

bool parseSlots(JsonParser &parser, KeyboardMacroSettings &settings,
                MacroSettingsError *error)
{
  if (!parser.consume('['))
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "slots", "slots must be a JSON array");
  }
  for (size_t i = 0; i < KeyboardMacroSettings::SlotCount; ++i)
  {
    if (!parseSlot(parser, i, settings, error))
    {
      return false;
    }
    if (i + 1 < KeyboardMacroSettings::SlotCount)
    {
      if (!parser.consume(','))
      {
        return fail(error, MacroSettingsErrorCode::InvalidJson,
                    "slots", "slots must contain exactly four entries");
      }
    }
  }
  if (!parser.consume(']'))
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "slots", "slots must contain exactly four entries");
  }
  return true;
}

class JsonWriter
{
public:
  JsonWriter(char *out, size_t capacity) : out_(out), capacity_(capacity)
  {
    if (out_ != nullptr && capacity_ > 0)
    {
      out_[0] = '\0';
    }
    else
    {
      valid_ = false;
    }
  }

  void literal(const char *value)
  {
    while (value != nullptr && *value != '\0')
    {
      byte(*value++);
    }
  }

  void number(uint32_t value)
  {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(value));
    literal(buffer);
  }

  void string(const char *value)
  {
    byte('"');
    for (size_t i = 0; value != nullptr && value[i] != '\0'; ++i)
    {
      const uint8_t current = static_cast<uint8_t>(value[i]);
      switch (current)
      {
      case '"': literal("\\\""); break;
      case '\\': literal("\\\\"); break;
      case '\b': literal("\\b"); break;
      case '\f': literal("\\f"); break;
      case '\n': literal("\\n"); break;
      case '\r': literal("\\r"); break;
      case '\t': literal("\\t"); break;
      default:
        if (current < 0x20)
        {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", current);
          literal(escaped);
        }
        else
        {
          byte(static_cast<char>(current));
        }
      }
    }
    byte('"');
  }

  size_t finish()
  {
    return valid_ ? used_ : 0;
  }

private:
  void byte(char value)
  {
    if (!valid_ || used_ + 1 >= capacity_)
    {
      valid_ = false;
      if (out_ != nullptr && capacity_ > 0)
      {
        out_[0] = '\0';
      }
      return;
    }
    out_[used_++] = value;
    out_[used_] = '\0';
  }

  char *out_ = nullptr;
  size_t capacity_ = 0;
  size_t used_ = 0;
  bool valid_ = true;
};
} // namespace

bool decodeKeyboardMacroSettingsJson(const char *json, size_t length,
                                     KeyboardMacroSettings &settings,
                                     MacroSettingsError *error)
{
  clearError(error);
  if (json == nullptr || length == 0)
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "json", "request body is not a JSON object");
  }
  JsonParser parser(json, length);
  if (!parser.consume('{'))
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "json", "request body is not a JSON object");
  }

  KeyboardMacroSettings candidate;
  uint8_t fields = 0;
  uint32_t schemaVersion = 0;
  bool afterComma = false;
  for (;;)
  {
    if (parser.consume('}'))
    {
      if (afterComma)
      {
        return fail(error, MacroSettingsErrorCode::InvalidJson,
                    "json", "JSON object must not end with a trailing comma");
      }
      break;
    }
    afterComma = false;
    char key[24] = {};
    if (!parser.string(key, sizeof(key)) || !parser.consume(':'))
    {
      return fail(error, MacroSettingsErrorCode::InvalidJson,
                  "json", "JSON object contains invalid syntax");
    }
    if (strcmp(key, "schemaVersion") == 0)
    {
      if (!markField(fields, 1u, error, "schemaVersion") ||
          !parser.unsignedInteger(schemaVersion))
      {
        return error != nullptr && error->code != MacroSettingsErrorCode::None
                   ? false
                   : fail(error, MacroSettingsErrorCode::InvalidJson,
                          "schemaVersion", "schemaVersion must be an unsigned integer");
      }
    }
    else if (strcmp(key, "slots") == 0)
    {
      if (!markField(fields, 2u, error, "slots") ||
          !parseSlots(parser, candidate, error))
      {
        return false;
      }
    }
    else
    {
      return fail(error, MacroSettingsErrorCode::UnknownJsonField,
                  "json", "JSON object contains an unknown field");
    }
    if (parser.consume('}'))
    {
      break;
    }
    if (!parser.consume(','))
    {
      return fail(error, MacroSettingsErrorCode::InvalidJson,
                  "json", "JSON object fields must be comma-separated");
    }
    afterComma = true;
  }
  if (!parser.done())
  {
    return fail(error, MacroSettingsErrorCode::InvalidJson,
                "json", "JSON object has trailing input");
  }
  if (fields != 3u)
  {
    return fail(error, MacroSettingsErrorCode::MissingJsonField,
                "json", "JSON object is missing a required field");
  }
  if (schemaVersion != KeyboardMacroSettings::CurrentVersion)
  {
    return fail(error, MacroSettingsErrorCode::UnsupportedSettingsVersion,
                "schemaVersion", "macro settings schema is not supported");
  }
  if (!candidate.validate(error))
  {
    return false;
  }
  settings = candidate;
  return true;
}

size_t encodeKeyboardMacroSettingsJson(const KeyboardMacroSettings &settings,
                                       char *out, size_t capacity,
                                       MacroSettingsError *error)
{
  clearError(error);
  if (!settings.validate(error))
  {
    return 0;
  }
  JsonWriter writer(out, capacity);
  writer.literal("{\"schemaVersion\":");
  writer.number(KeyboardMacroSettings::CurrentVersion);
  writer.literal(",\"slots\":[");
  for (size_t i = 0; i < KeyboardMacroSettings::SlotCount; ++i)
  {
    if (i != 0) writer.literal(",");
    writer.literal("{\"triggerUsage\":");
    writer.number(settings.triggerUsage(i));
    writer.literal(",\"text\":");
    writer.string(settings.text(i));
    writer.literal("}");
  }
  writer.literal("]}");
  const size_t written = writer.finish();
  if (written == 0)
  {
    fail(error, MacroSettingsErrorCode::InvalidRecordSize,
         "json", "JSON output buffer is too small");
  }
  return written;
}

} // namespace esp32keybridge
