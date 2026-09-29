#include "ESP32KeyBridgeSettingsJson.h"

#include <stdio.h>
#include <string.h>

namespace esp32keybridge
{
namespace
{
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

class JsonParser
{
public:
  JsonParser(const char *json, size_t length) : cursor_(json), end_(json + length) {}

  void whitespace()
  {
    while (cursor_ < end_ &&
           (*cursor_ == ' ' || *cursor_ == '\t' || *cursor_ == '\r' || *cursor_ == '\n'))
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
    size_t length = 0;
    while (cursor_ < end_ && *cursor_ != '"')
    {
      const unsigned char value = static_cast<unsigned char>(*cursor_);
      if (value < 0x20 || value >= 0x7f || *cursor_ == '\\' || length + 1 >= capacity)
      {
        return false;
      }
      out[length++] = *cursor_++;
    }
    if (cursor_ >= end_ || *cursor_ != '"')
    {
      return false;
    }
    ++cursor_;
    out[length] = '\0';
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

  bool boolean(bool &value)
  {
    whitespace();
    if (remainingStartsWith("true"))
    {
      cursor_ += 4;
      value = true;
      return true;
    }
    if (remainingStartsWith("false"))
    {
      cursor_ += 5;
      value = false;
      return true;
    }
    return false;
  }

  bool done()
  {
    whitespace();
    return cursor_ == end_;
  }

private:
  bool remainingStartsWith(const char *value) const
  {
    size_t i = 0;
    while (value[i] != '\0')
    {
      if (cursor_ + i >= end_ || cursor_[i] != value[i])
      {
        return false;
      }
      ++i;
    }
    return true;
  }

  const char *cursor_;
  const char *end_;
};

enum FieldMask : uint8_t
{
  SchemaVersionField = 1u << 0,
  InputLayoutField = 1u << 1,
  HostLayoutField = 1u << 2,
  ConversionEnabledField = 1u << 3,
  ToggleUsageField = 1u << 4,
  AllFields = SchemaVersionField | InputLayoutField | HostLayoutField |
              ConversionEnabledField | ToggleUsageField,
};

bool markField(uint8_t &mask, uint8_t field, LayoutSettingsError *error,
               const char *fieldName)
{
  if ((mask & field) != 0)
  {
    return fail(error, LayoutSettingsErrorCode::DuplicateJsonField,
                fieldName, "JSON field appears more than once");
  }
  mask |= field;
  return true;
}
} // namespace

bool decodeKeyboardLayoutSettingsJson(const char *json, size_t length,
                                      KeyboardLayoutSettings &settings,
                                      LayoutSettingsError *error)
{
  clearError(error);
  if (json == nullptr || length == 0)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidJson,
                "json", "request body is not a JSON object");
  }

  JsonParser parser(json, length);
  if (!parser.consume('{'))
  {
    return fail(error, LayoutSettingsErrorCode::InvalidJson,
                "json", "request body is not a JSON object");
  }

  uint8_t fields = 0;
  uint32_t schemaVersion = 0;
  uint32_t toggleUsage = 0;
  char inputLayout[KeyboardLayoutSettings::MaxLayoutNameLength + 1] = {};
  char hostLayout[KeyboardLayoutSettings::MaxLayoutNameLength + 1] = {};
  bool conversionEnabled = false;
  bool afterComma = false;

  for (;;)
  {
    if (parser.consume('}'))
    {
      if (afterComma)
      {
        return fail(error, LayoutSettingsErrorCode::InvalidJson,
                    "json", "JSON object must not end with a trailing comma");
      }
      break;
    }
    afterComma = false;

    char key[40] = {};
    if (!parser.string(key, sizeof(key)) || !parser.consume(':'))
    {
      return fail(error, LayoutSettingsErrorCode::InvalidJson,
                  "json", "JSON object contains invalid syntax");
    }

    if (strcmp(key, "schemaVersion") == 0)
    {
      if (!markField(fields, SchemaVersionField, error, "schemaVersion") ||
          !parser.unsignedInteger(schemaVersion))
      {
        return error != nullptr && error->code != LayoutSettingsErrorCode::None
                   ? false
                   : fail(error, LayoutSettingsErrorCode::InvalidJson,
                          "schemaVersion", "schemaVersion must be an unsigned integer");
      }
    }
    else if (strcmp(key, "inputLayout") == 0)
    {
      if (!markField(fields, InputLayoutField, error, "inputLayout") ||
          !parser.string(inputLayout, sizeof(inputLayout)))
      {
        return error != nullptr && error->code != LayoutSettingsErrorCode::None
                   ? false
                   : fail(error, LayoutSettingsErrorCode::InvalidJson,
                          "inputLayout", "inputLayout must be a short ASCII string");
      }
    }
    else if (strcmp(key, "hostLayout") == 0)
    {
      if (!markField(fields, HostLayoutField, error, "hostLayout") ||
          !parser.string(hostLayout, sizeof(hostLayout)))
      {
        return error != nullptr && error->code != LayoutSettingsErrorCode::None
                   ? false
                   : fail(error, LayoutSettingsErrorCode::InvalidJson,
                          "hostLayout", "hostLayout must be a short ASCII string");
      }
    }
    else if (strcmp(key, "layoutConversionEnabled") == 0)
    {
      if (!markField(fields, ConversionEnabledField, error,
                     "layoutConversionEnabled") ||
          !parser.boolean(conversionEnabled))
      {
        return error != nullptr && error->code != LayoutSettingsErrorCode::None
                   ? false
                   : fail(error, LayoutSettingsErrorCode::InvalidJson,
                          "layoutConversionEnabled",
                          "layoutConversionEnabled must be true or false");
      }
    }
    else if (strcmp(key, "layoutConversionToggleUsage") == 0)
    {
      if (!markField(fields, ToggleUsageField, error,
                     "layoutConversionToggleUsage") ||
          !parser.unsignedInteger(toggleUsage))
      {
        return error != nullptr && error->code != LayoutSettingsErrorCode::None
                   ? false
                   : fail(error, LayoutSettingsErrorCode::InvalidJson,
                          "layoutConversionToggleUsage",
                          "layoutConversionToggleUsage must be an unsigned integer");
      }
    }
    else
    {
      return fail(error, LayoutSettingsErrorCode::UnknownJsonField,
                  "json", "JSON object contains an unknown field");
    }

    if (parser.consume('}'))
    {
      break;
    }
    if (!parser.consume(','))
    {
      return fail(error, LayoutSettingsErrorCode::InvalidJson,
                  "json", "JSON object fields must be comma-separated");
    }
    afterComma = true;
  }

  if (!parser.done())
  {
    return fail(error, LayoutSettingsErrorCode::InvalidJson,
                "json", "JSON object has trailing input");
  }
  if (fields != AllFields)
  {
    return fail(error, LayoutSettingsErrorCode::MissingJsonField,
                "json", "JSON object is missing a required field");
  }
  if (schemaVersion != KeyboardLayoutSettings::CurrentVersion)
  {
    return fail(error, LayoutSettingsErrorCode::UnsupportedSettingsVersion,
                "schemaVersion", "settings schema is not supported");
  }
  if (toggleUsage > 0xffffu)
  {
    return fail(error, LayoutSettingsErrorCode::InvalidToggleUsage,
                "layoutConversionToggleUsage", "toggle usage is out of range");
  }

  KeyboardLayoutSettings decoded;
  if (!decoded.setInputLayoutName(inputLayout, error) ||
      !decoded.setHostLayoutName(hostLayout, error) ||
      !decoded.setLayoutConversionToggleUsage(static_cast<uint16_t>(toggleUsage), error))
  {
    return false;
  }
  decoded.setLayoutConversionEnabled(conversionEnabled);
  if (!decoded.validate(error))
  {
    return false;
  }

  settings = decoded;
  return true;
}

size_t encodeKeyboardLayoutSettingsJson(const KeyboardLayoutSettings &settings,
                                        char *out, size_t capacity,
                                        LayoutSettingsError *error)
{
  clearError(error);
  if (out == nullptr || capacity == 0)
  {
    fail(error, LayoutSettingsErrorCode::InvalidRecordSize,
         "json", "JSON output buffer is empty");
    return 0;
  }
  if (!settings.validate(error))
  {
    return 0;
  }
  const int length = snprintf(
      out, capacity,
      "{\"schemaVersion\":%lu,\"inputLayout\":\"%s\","
      "\"hostLayout\":\"%s\",\"layoutConversionEnabled\":%s,"
      "\"layoutConversionToggleUsage\":%u}",
      static_cast<unsigned long>(KeyboardLayoutSettings::CurrentVersion),
      settings.inputLayoutName(), settings.hostLayoutName(),
      settings.layoutConversionEnabled() ? "true" : "false",
      static_cast<unsigned>(settings.layoutConversionToggleUsage()));
  if (length < 0 || static_cast<size_t>(length) >= capacity)
  {
    out[0] = '\0';
    fail(error, LayoutSettingsErrorCode::InvalidRecordSize,
         "json", "JSON output buffer is too small");
    return 0;
  }
  return static_cast<size_t>(length);
}

} // namespace esp32keybridge
