// esp-rack-light-demo — Phase 1.0 minimum bring-up.
//
// At this stage there's no LightControlModule yet — the goal is just
// to prove the ESPRack composition root (Builder + App + Module
// lifecycle) compiles and boots with a single WiFi module installed.
// Once that's verified, LightControlModule slots in with one
// .install<>() line.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <ESPRack.h>
#include <WiFiModule.h>

AsyncWebServer server(80);
std::unique_ptr<ESPRack::App> app;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("esp-rack-light-demo — Phase 1.0 boot"));

  app = ESPRack::Builder(&server, "ESPRackDemo", "v0.1.0-pre")
    .install<WiFiModule>()
    .build();

  app->begin();
  Serial.println(F("[demo] App.begin() returned"));
}

void loop() {
  if (app) app->loop();
}
