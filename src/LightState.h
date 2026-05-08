#pragma once
#ifndef LIGHT_STATE_H
#define LIGHT_STATE_H

// LightState — pure data + static-method declarations for the
// LightStateService demo's persistence + form schema.
//
// Why split from LightStateService:
//   * `read()` is invoked by ConfigDelegate's secret-key probe on a
//     default-constructed state, so it MUST NOT touch service-runtime
//     fields. Lifting the struct + its static methods into their own
//     file makes the constraint visible at the file boundary, not just
//     by `static` keyword convention inside one big .cpp.
//   * Form schema (read/readSta) is the single biggest chunk of code
//     in the service; isolating it lets the service .cpp focus on
//     ctor / begin / loop / handlers without 400 lines of FormBuilder
//     calls obscuring the runtime flow.
//   * Tests can exercise readFs/updFs/read/readSta in isolation —
//     pure JSON-in / JSON-out, no ConfigDelegate or WebManager mocks.
//
// LightState lives in the consumer repo, NOT in ESPRack: this is
// the "thing the framework hosts", not a reusable framework piece.
// Forms exercise every widget type the framework exposes (text,
// number, slider, checkbox, switch, button, dropdown, radio,
// textarea, files, upload, datetime, action, trend, table) so a
// flag-flip in features.ini gives a complete UI smoke test.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <StatefulService.h>
#include <vector>

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
  // Topics that mutated since last WS tick (for streaming WS push,
  // same upsert-by-topic pattern as MqttSettingsService).
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

  // ── Static glue for ConfigDelegate + WebFeatureEntry ─────────────
  // All implementations in LightState.cpp. Splitting forms / WS push
  // / FS persistence into one file keeps the service .cpp focused on
  // runtime concerns.
  //
  // readSta / updateSta — WS frame readers (live tab cadence).
  static void readSta(LightState& s, JsonObject& root);
  static StateUpdateResult updateSta(JsonObject& root, LightState& s);

  // read / update — REST GET form schema + REST POST handler.
  // ConfigDelegate ALSO calls read() on a probe state during
  // registerConfig to discover secret keys; the form schema must
  // therefore be safe to invoke against a default-constructed
  // LightState (no service-runtime field accesses leak in here).
  static void read(LightState& s, JsonObject& root);
  static StateUpdateResult update(JsonObject& root, LightState& s);

  // readFs / updFs — disk persistence shape (subset of the in-memory
  // state — chart stream / mqtt_inbox / attached_view are all
  // volatile, only persisted-editable fields go to FS).
  static void readFs(LightState& s, JsonObject& root);
  static StateUpdateResult updFs(JsonObject& root, LightState& s);
};

#endif  // LIGHT_STATE_H
