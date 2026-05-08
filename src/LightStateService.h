#pragma once
#ifndef LIGHT_STATE_SERVICE_H
#define LIGHT_STATE_SERVICE_H

// LightStateService — runtime that hosts a LightState behind a
// ConfigDelegate + WebFeatureEntry, drives a sin/cos chart tick,
// and bridges into the framework's Telegram + MQTT subscription
// providers. Pure "what happens at runtime" — the form schema and
// the persisted-shape readers live next to the data in
// LightState.{h,cpp} (see header comment there for the why).
//
// Responsibilities of this file:
//   * ctor: register with WebManager, mount three actions
//   * begin: load config, subscribe to Telegram + MQTT
//   * loop: advance phase, push chart updates, mirror attached_view
//   * upsertInbox: feed the live-key-value map fed by MQTT '#'
//
// Anything FormBuilder-related lives in LightState.cpp.

#include <Arduino.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <map>

#include <ConfigManager.h>
#include <ConfigDelegate.h>
#include <WebFeatureDelegate.h>
#include <WebManager.h>
#include <TelegramSubscription.h>
#include <MqttSubscription.h>

#include "LightState.h"

class ITelegramProvider;
class IMqttProvider;

#define LIGHT_SETTINGS_PATH        "/rest/lightState"
#define LIGHT_SETTINGS_SOCKET_PATH "/ws/lightState"
#define LIGHT_SETTINGS_FILE        "/config/lightState.json"

class LightStateService : public StatefulService<LightState> {
 public:
  // `telegram` and `mqtt` are optional — when supplied (the
  // respective module was installed in the Builder chain)
  // LightStateService subscribes and streams a sin/cos sample once
  // a minute as a live end-to-end demo of the subscription provider
  // model. Same cadence on both transports — operators can toggle
  // each side independently to compare delivery.
  LightStateService(ConfigManager* cfgMgr,
                    WebManager* web,
                    ITelegramProvider* telegram = nullptr,
                    IMqttProvider* mqtt = nullptr);

  void begin();
  void loop();

  void setVersion(const char* v) { _version = v ? String(v) : String(); }

  // Demonstrates the consumer pattern from the IMqttProvider model:
  // any other code in the service can read the last-seen payload for
  // a given topic without polling the provider. Returns "" when the
  // topic was never received.
  String valueOf(const String& topic) const {
    auto it = _values.find(topic);
    return it != _values.end() ? it->second : String();
  }

 private:
  // Live key-value store fed by MQTT onMessage. Backs valueOf() and
  // the mqtt_inbox UI table. Bounded by upsertInbox to MAX_INBOX
  // entries — operator's attached topics flow in here, but a stray
  // wildcard handler on a busy broker shouldn't unbounded-grow heap.
  static constexpr size_t MAX_INBOX = 32;
  std::map<String, String> _values;
  void upsertInbox(const String& topic, const String& payload);

  ConfigDelegate<LightState>      _cfg;
  WebFeatureEntry<LightState>*    _feature{nullptr};
  WebManager*                     _web{nullptr};
  ITelegramProvider*              _telegram{nullptr};
  TelegramSubscription            _telegramSub;
  unsigned long                   _telegramLastSendMs{0};
  IMqttProvider*                  _mqtt{nullptr};
  MqttSubscription                _mqttSub;
  MqttSubscription                _mqttSniffer;   // separate handle for "#" listening
  unsigned long                   _mqttLastPubMs{0};
  double                          _phase{0.0};
  unsigned long                   _lastTick{0};
  String                          _version;
};

#endif  // LIGHT_STATE_SERVICE_H
