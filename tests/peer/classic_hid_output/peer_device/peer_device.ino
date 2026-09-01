#include <EspBleClassic.h>

EspBleClassic bluetooth;

void printHex(const String &value)
{
  for (size_t i = 0; i < value.length(); ++i)
    Serial.printf("%02x", static_cast<uint8_t>(value[i]));
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  bluetooth.hidHost().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HOST_CONNECTED peer=%s\n", event.peerAddress.c_str());
  });
  bluetooth.hidHost().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("HOST_DISCONNECTED");
  });
  bluetooth.hidHost().onKeyboardState([](const EspBleClassicHidKeyboardState &state) {
    Serial.printf("KEY_STATE modifiers=%02x a=%u b=%u c=%u d=%u e=%u f=%u\n",
      state.modifiers, state.isDown(0x04), state.isDown(0x05), state.isDown(0x06),
      state.isDown(0x07), state.isDown(0x08), state.isDown(0x09));
  });
  bluetooth.hidHost().onMouse([](const EspBleClassicHidMouseEvent &event) {
    Serial.printf("MOUSE buttons=%02x x=%d y=%d wheel=%d\n",
      event.buttons, event.x, event.y, event.wheel);
  });
  bluetooth.hidHost().onInputReport([](const EspBleClassicHidReport &event) {
    Serial.printf("RAW id=%u hex=", event.reportId);
    printHex(event.value);
    Serial.println();
  });
  EspBleClassicConfig config;
  config.deviceName = "KeyBridge Classic Output Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("HOST_INIT_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("HOST_READY");
}

void loop()
{
  bluetooth.update();
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
      Serial.printf("HOST_CONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().connect(command.c_str() + 1) ? 1 : 0);
    else if (command == "l")
      Serial.printf("HOST_LEDS_WRITTEN %u\n",
        bluetooth.hidHost().setKeyboardLeds(false, true, false) ? 1 : 0);
    else if (command == "d")
      Serial.printf("HOST_DISCONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().disconnect() ? 1 : 0);
    else if (command == "a") Serial.println("HOST_READY");
  }
  delay(1);
}
