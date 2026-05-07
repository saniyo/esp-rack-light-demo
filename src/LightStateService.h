#pragma once
#ifndef LIGHT_STATE_SERVICE_H
#define LIGHT_STATE_SERVICE_H

// LightStateService — application-specific demo of every FormBuilder
// widget driven by a sin/cos generator. Lives in the consumer repo,
// NOT in ESPRack: this is the "thing the framework hosts", not a
// reusable framework piece.
//
// The original ESP-SES-LightService incarnation took an ESPReact*
// constructor argument and pulled mqtt/ntp/autoUpdate pointers off
// it; in the post-refactor design those were already dead members
// (stored in init list, never re-read). Stripped here. What remains:
//
//   * ConfigDelegate<LightState>  — persistence to /config/lightState.json
//   * WebFeatureEntry<LightState> — REST + WS surface registered with WebManager
//   * loop()                       — sin/cos phase advance + WS broadcast
//   * three demo actions           — light.reload / light.reset / light.ping
//
// Forms exercise every widget type the framework exposes (text, number,
// slider, checkbox, switch, button, dropdown, radio, textarea, files,
// upload, datetime, action, trend, table) so a flag-flip in features.ini
// gives a complete UI smoke test.

#include <Arduino.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <vector>

#include <FormBuilder.h>
#include <ConfigManager.h>
#include <ConfigDelegate.h>
#include <WebFeatureDelegate.h>
#include <WebManager.h>
#include <TelegramSubscription.h>
#include <MqttSubscription.h>
#include <map>

class ITelegramProvider;
class IMqttProvider;

#define LIGHT_SETTINGS_PATH        "/rest/lightState"
#define LIGHT_SETTINGS_SOCKET_PATH "/ws/lightState"
#define LIGHT_SETTINGS_FILE        "/config/lightState.json"

class LightState {
 public:
  // Test-bench fields — one per widget type, persisted via readFs/updFs.
  String t_text     {"hello world"};
  double t_number   {42.5};
  double t_slider   {50.0};
  bool   t_checkbox {false};
  bool   t_switch   {true};
  bool   t_button   {false};
  int    t_dropdown {1};
  int    t_radio    {2};
  String t_textarea {"multi\nline\ntext"};
  String t_files    {"/flash"};
  String t_upload   {""};

  int32_t t_datetime {1735689600};
  int32_t t_date     {1735689600};
  int32_t t_time     {1735689600};

  // Live chart stream. tick_ms is the WS broadcast cadence — exposed on
  // the Settings tab as a slider so the operator can stress-test
  // throughput without rebuilding firmware (10..1000 ms range).
  double   chart_sin {0.0};
  double   chart_cos {1.0};
  uint32_t chart_ts  {0};
  uint16_t tick_ms   {100};

  uint32_t uptime_ms {0};

  // MQTT received values — populated by LightStateService's onMessage
  // handler (subscribes to "#" with empty prefix). Keyed by topic,
  // value is the most recent payload. Demonstrates the consumer
  // pattern: handler stores into C++ heap, anywhere in the service
  // can read via valueOf("topic"). Surfaced in the Status tab as a
  // "MQTT inbox" table so the operator sees attached topics + their
  // last values without leaving the Light page.
  struct MqttInboxEntry {
    String   topic;
    String   value;
    uint32_t count{0};
    uint32_t lastSeenAt_s{0};
  };
  std::vector<MqttInboxEntry> mqtt_inbox;
  // Index of the entry mutated since last WS tick (for streaming WS
  // push, same upsert-by-topic pattern as MqttSettingsService).
  std::vector<String> mqtt_pending;

  // Snapshot of operator-attached topics (from IMqttProvider::
  // attachedTopics()) joined with the last-seen value from the
  // service's _values map. Refreshed once per loop() tick so the
  // form reader sees a consistent view without having to reach into
  // a non-static service field. Bounded by MQTT_ATTACHED_TOPICS_MAX
  // on the provider side; we just mirror.
  struct AttachedViewEntry {
    String topic;
    String value;
  };
  std::vector<AttachedViewEntry> attached_view;

  // Rolling table feeding the TABLE widget via WS append-mode.
  struct TableRow {
    uint32_t idx;
    uint32_t ts;
    double   sin_v;
    double   cos_v;
  };
  std::vector<TableRow> table_rows;
  uint32_t table_next_idx {0};

  static void readSta(LightState& s, JsonObject& root);
  static StateUpdateResult updateSta(JsonObject& root, LightState& s);

  static void read(LightState& s, JsonObject& root);
  static StateUpdateResult update(JsonObject& root, LightState& s);

  static void readFs(LightState& s, JsonObject& root);
  static StateUpdateResult updFs(JsonObject& root, LightState& s);
};

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
