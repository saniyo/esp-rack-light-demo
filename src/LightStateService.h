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

class ITelegramProvider;

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
  // `telegram` is optional — when supplied (TelegramModule was
  // installed in the Builder chain) LightStateService will subscribe
  // and stream a sin/cos sample once a minute as a live demo of the
  // subscription provider model.
  LightStateService(ConfigManager* cfgMgr,
                    WebManager* web,
                    ITelegramProvider* telegram = nullptr);

  void begin();
  void loop();

  void setVersion(const char* v) { _version = v ? String(v) : String(); }

 private:
  ConfigDelegate<LightState>      _cfg;
  WebFeatureEntry<LightState>*    _feature{nullptr};
  WebManager*                     _web{nullptr};
  ITelegramProvider*              _telegram{nullptr};
  TelegramSubscription            _telegramSub;
  unsigned long                   _telegramLastSendMs{0};
  double                          _phase{0.0};
  unsigned long                   _lastTick{0};
  String                          _version;
};

#endif  // LIGHT_STATE_SERVICE_H
