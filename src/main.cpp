// esp-rack-light-demo — Phase 1.5 boot.
//
// Composition root for the reference consumer of ESPRack. Installs
// every module that's been ported so far. The Builder topologically
// sorts by priority + dependencies; the call-order of .install<>()
// is irrelevant to runtime ordering.
//
// Still pending:
//   * SecurityModule          — login + JWT (FT_SECURITY=0 today)
//   * LightControl module     — the actual application
//   * AuthenticationService   — bundled with SecurityModule
//
// Once SecurityModule lands we'll flip FT_SECURITY=1 in features.ini
// and the React frontend will reactivate the login screen.

#include <Arduino.h>
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

AsyncWebServer server(80);
std::unique_ptr<ESPRack::App> app;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("esp-rack-light-demo — Phase 1.5 boot"));

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
