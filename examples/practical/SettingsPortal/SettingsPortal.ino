// Phase 2 bring-up for the ESP32-S3 local settings website.
//
// Connect a phone to the "KeyBridge-Setup" Wi-Fi network with password
// "keybridge", then open http://192.168.4.1/. The page is read-only in this
// phase. Change both credentials before using this beyond local development.

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgePreferences.h>
#include <ESP32KeyBridgeSettings.h>
#include <ESP32KeyBridgeWeb.h>

esp32keybridge::ESP32KeyBridge bridge;
esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;
esp32keybridge::KeyboardLayoutSettings layoutSettings;
esp32keybridge::PreferencesLayoutSettingsStorage settingsNvs;
esp32keybridge::KeyboardLayoutSettingsStore settingsStore(settingsNvs);
esp32keybridge::Esp32S3SettingsWebServer settingsWeb(layoutSettings, bridge);

namespace
{
const char *loadStatusName(esp32keybridge::LayoutSettingsLoadStatus status)
{
  using Status = esp32keybridge::LayoutSettingsLoadStatus;
  switch (status)
  {
  case Status::Loaded:
    return "loaded";
  case Status::LoadedRecovered:
    return "recovered previous record";
  case Status::DefaultsNoSavedSettings:
    return "defaults (first boot)";
  case Status::DefaultsInvalidSettings:
    return "defaults (stored settings invalid)";
  case Status::DefaultsStorageError:
    return "defaults (NVS read failed)";
  }
  return "unknown";
}
} // namespace

void setup()
{
  Serial.begin(115200);

  esp32keybridge::LayoutSettingsError settingsError;
  const esp32keybridge::LayoutSettingsLoadStatus loadStatus =
      settingsStore.loadOrDefault(layoutSettings, &settingsError);
  Serial.printf("Settings: %s\n", loadStatusName(loadStatus));
  if (settingsError.message != nullptr)
  {
    Serial.printf("Settings detail: %s\n", settingsError.message);
  }

  // Phase 1 settings are applied through the same core API the later write
  // endpoint will use. A real keyboard adapter binds to keyboardConfig.
  esp32keybridge::InputConfig &keyboardConfig = bridgeConfig.addInputConfig();
  if (!layoutSettings.apply(bridge, bridgeConfig, keyboardConfig, &settingsError))
  {
    Serial.printf("Settings apply failed: %s\n",
                  settingsError.message != nullptr ? settingsError.message : "unknown");
    return;
  }

  esp32keybridge::SettingsAccessPointConfig accessPoint;
  accessPoint.ssid = "KeyBridge-Setup";
  accessPoint.password = "keybridge";
  accessPoint.maxConnections = 1;

  esp32keybridge::SettingsWebError webError;
  if (!settingsWeb.begin(accessPoint, &webError))
  {
    Serial.printf("Settings website failed: %s\n",
                  webError.message != nullptr ? webError.message : "unknown");
    return;
  }

  Serial.printf("Wi-Fi: %s\n", accessPoint.ssid);
  Serial.printf("Password: %s\n", accessPoint.password);
  Serial.printf("Open: http://%s/\n", settingsWeb.address().toString().c_str());
}

void loop()
{
  // Keep keyboard processing first when integrating this into a real bridge.
  bridge.update();
  settingsWeb.update();
  delay(1);
}
