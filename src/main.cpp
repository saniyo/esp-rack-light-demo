// esp-rack-light-demo — Phase 1.7 boot.
//
// Composition root for the reference consumer of ESPRack. Installs
// the framework modules + the demo's own LightControlModule. The
// Builder topologically sorts by priority + dependencies; the call-
// order of .install<>() is irrelevant to runtime ordering.
//
// All planned modules are now present. LightControlModule lives in
// THIS repo (application code), not in ESPRack — flip FT_PROJECT=0
// in features.ini to mute it without touching this file.

#include <Arduino.h>
#include <esp_log.h>
#include <ESPAsyncWebServer.h>

#include <ESPRack.h>

#include <WiFiModule.h>
#include <APModule.h>
#include <SecurityModule.h>
#include <FeaturesModule.h>
#include <UiDynModule.h>
#include <PresenceModule.h>
#include <RestartModule.h>
#include <FactoryResetModule.h>
#include <FileSystemModule.h>
#include <SystemStatusModule.h>
#include <WsDiagModule.h>
#include <ConfigManagerUiModule.h>
#include <WebEndpointsModule.h>
#include <NTPModule.h>
#include <OTAModule.h>
#include <UploadFirmwareModule.h>
#include <AutoUpdateModule.h>
#include <MqttModule.h>
#include <TelegramModule.h>
#include <CertManagerModule.h>

#include "LightControlModule.h"

AsyncWebServer server(80);
std::unique_ptr<ESPRack::App> app;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("esp-rack-light-demo — Phase 1.5 boot"));

  // The PING / PINGRESP / _onData spam from AsyncMqttClient is
  // gated by Arduino-ESP32's compile-time log_i() macro, controlled
  // by CORE_DEBUG_LEVEL in platformio.ini (set to 2 = WARN to
  // silence). This runtime call covers code paths that use the
  // ESP_LOGx family (which IS runtime-tunable per tag), so it's
  // kept as a safety net for any lib that later switches to
  // ESP_LOGI under the same tag.
  esp_log_level_set("AsyncMqttClient", ESP_LOG_WARN);

  app = ESPRack::Builder(&server, "ESPRackDemo", "v0.1.6-pre")
    // foundation: feature flags, UI manifest, presence registry
    .install<FeaturesModule>()
    .install<UiDynModule>()
    .install<PresenceModule>()
    // security (real PBKDF2 + JWT — replaces NullSecurityManager)
    .install<SecurityModule>()
    // filesystem (LittleFS/SD backends + file API)
    .install<FileSystemModule>()
    // network radio (STA + AP)
    .install<WiFiModule>()
    .install<APModule>()
    // status / diagnostics
    .install<SystemStatusModule>()
    .install<WsDiagModule>()
    // system tabs
    .install<ConfigManagerUiModule>()
    .install<WebEndpointsModule>()
    // network features
    .install<NTPModule>()
    .install<OTAModule>()
    .install<UploadFirmwareModule>()
    .install<AutoUpdateModule>()
    .install<MqttModule>()
    .install<TelegramModule>()
    .install<CertManagerModule>()
    // application
    .install<LightControlModule>()
    // actions
    .install<RestartModule>()
    .install<FactoryResetModule>()
    .build();

  app->begin();
  Serial.println(F("[demo] App.begin() returned"));
}

void loop() {
  if (app) app->loop();
}
