#pragma once

#include "ESP32KeyBridgeBluetooth.h"

#include <stdint.h>

namespace esp32keybridge
{

struct RgbColor
{
  constexpr RgbColor(uint8_t redValue = 0, uint8_t greenValue = 0,
                     uint8_t blueValue = 0)
      : red(redValue), green(greenValue), blue(blueValue) {}
  bool operator==(const RgbColor &other) const
  {
    return red == other.red && green == other.green && blue == other.blue;
  }
  bool operator!=(const RgbColor &other) const { return !(*this == other); }
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

enum class BluetoothStatusLedPattern : uint8_t
{
  Off = 0,
  Boot,
  Connected,
  Searching,
  Pairing,
  Disconnected,
  Destructive,
  Error,
};

struct BluetoothStatusLedFrame
{
  BluetoothStatusLedPattern pattern = BluetoothStatusLedPattern::Off;
  RgbColor color;
};

inline RgbColor bluetoothProfileColor(size_t slot)
{
  constexpr uint8_t MaximumChannel = 32;
  if (slot == 2) return RgbColor(0, MaximumChannel, 0);
  if (slot == 3) return RgbColor(MaximumChannel, 0, MaximumChannel);
  return RgbColor(0, 0, MaximumChannel);
}

inline RgbColor scaleBluetoothStatusColor(RgbColor color, uint8_t level)
{
  return RgbColor(static_cast<uint8_t>(color.red * level / 32),
                  static_cast<uint8_t>(color.green * level / 32),
                  static_cast<uint8_t>(color.blue * level / 32));
}

inline BluetoothStatusLedFrame bluetoothStatusLedFrame(
    const BluetoothTargetSnapshot &snapshot, uint32_t uptimeMs,
    bool bluetoothAvailable, bool destructiveReset)
{
  constexpr uint8_t MaximumChannel = 32;
  BluetoothStatusLedFrame frame;
  if (!bluetoothAvailable) return frame;
  if (uptimeMs < 800)
  {
    frame.pattern = BluetoothStatusLedPattern::Boot;
    const uint32_t step = uptimeMs / 200;
    frame.color = step == 0 ? RgbColor(MaximumChannel, MaximumChannel, MaximumChannel)
        : step == 1 ? RgbColor(MaximumChannel, 0, 0)
        : step == 2 ? RgbColor(0, MaximumChannel, 0)
                    : RgbColor(0, 0, MaximumChannel);
    return frame;
  }
  if (destructiveReset)
  {
    frame.pattern = BluetoothStatusLedPattern::Destructive;
    frame.color = uptimeMs % 300 < 150 ? RgbColor(MaximumChannel, 12, 0) : RgbColor();
    return frame;
  }
  if (snapshot.status == BluetoothTargetStatus::Failed || snapshot.error[0] != '\0')
  {
    frame.pattern = BluetoothStatusLedPattern::Error;
    const uint32_t phase = (uptimeMs - 800) % 1500;
    frame.color = phase < 750 && (phase / 150) % 2 == 0
        ? RgbColor(MaximumChannel, 0, 0) : RgbColor();
    return frame;
  }

  const RgbColor profile = bluetoothProfileColor(snapshot.selectedSlot);
  if (snapshot.operation == BluetoothProfileOperation::Pairing ||
      snapshot.status == BluetoothTargetStatus::Pairing)
  {
    frame.pattern = BluetoothStatusLedPattern::Pairing;
    frame.color = (uptimeMs / 300) % 2 == 0
        ? profile : RgbColor(MaximumChannel, MaximumChannel, MaximumChannel);
  }
  else if (snapshot.operation == BluetoothProfileOperation::Connecting ||
           snapshot.status == BluetoothTargetStatus::Switching)
  {
    frame.pattern = BluetoothStatusLedPattern::Searching;
    const uint32_t phase = (uptimeMs - 800) % 2000;
    const uint32_t ramp = phase <= 1000 ? phase : 2000 - phase;
    const uint8_t level = static_cast<uint8_t>(4 + ramp * 28 / 1000);
    frame.color = scaleBluetoothStatusColor(profile, level);
  }
  else if (snapshot.status == BluetoothTargetStatus::Connected ||
           snapshot.connectedCount != 0)
  {
    frame.pattern = BluetoothStatusLedPattern::Connected;
    frame.color = profile;
  }
  else
  {
    frame.pattern = BluetoothStatusLedPattern::Disconnected;
    frame.color = uptimeMs % 3000 < 120 ? profile : RgbColor();
  }
  return frame;
}

inline const char *bluetoothStatusLedPatternName(BluetoothStatusLedPattern pattern)
{
  switch (pattern)
  {
    case BluetoothStatusLedPattern::Boot: return "boot";
    case BluetoothStatusLedPattern::Connected: return "connected";
    case BluetoothStatusLedPattern::Searching: return "searching";
    case BluetoothStatusLedPattern::Pairing: return "pairing";
    case BluetoothStatusLedPattern::Disconnected: return "disconnected";
    case BluetoothStatusLedPattern::Destructive: return "resetting";
    case BluetoothStatusLedPattern::Error: return "error";
    case BluetoothStatusLedPattern::Off:
    default: return "off";
  }
}

} // namespace esp32keybridge
