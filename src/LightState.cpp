#include "LightState.h"

#include <FormBuilder.h>

// ===== FS config (persisted editable fields only — chart stream is volatile) =====
void LightState::readFs(LightState& s, JsonObject& root) {
  root["t_text"]     = s.t_text;
  root["t_number"]   = s.t_number;
  root["t_slider"]   = s.t_slider;
  root["t_checkbox"] = s.t_checkbox;
  root["t_switch"]   = s.t_switch;
  root["t_button"]   = s.t_button;
  root["t_dropdown"] = s.t_dropdown;
  root["t_radio"]    = s.t_radio;
  root["t_textarea"] = s.t_textarea;
  root["t_files"]    = s.t_files;
  root["t_upload"]   = s.t_upload;
  root["t_datetime"] = s.t_datetime;
  root["t_date"]     = s.t_date;
  root["t_time"]     = s.t_time;
  root["tick_ms"]    = s.tick_ms;
}

StateUpdateResult LightState::updFs(JsonObject& root, LightState& s) {
  bool ch = false;
  ch |= FormBuilder::updateValue(root, "t_text",     s.t_text);
  ch |= FormBuilder::updateValue(root, "t_number",   s.t_number);
  ch |= FormBuilder::updateValue(root, "t_slider",   s.t_slider);
  ch |= FormBuilder::updateValue(root, "t_checkbox", s.t_checkbox);
  ch |= FormBuilder::updateValue(root, "t_switch",   s.t_switch);
  ch |= FormBuilder::updateValue(root, "t_button",   s.t_button);
  ch |= FormBuilder::updateValue(root, "t_dropdown", s.t_dropdown);
  ch |= FormBuilder::updateValue(root, "t_radio",    s.t_radio);
  ch |= FormBuilder::updateValue(root, "t_textarea", s.t_textarea);
  ch |= FormBuilder::updateValue(root, "t_files",    s.t_files);
  ch |= FormBuilder::updateValue(root, "t_upload",   s.t_upload);
  ch |= FormBuilder::updateValue(root, "t_datetime", s.t_datetime);
  ch |= FormBuilder::updateValue(root, "t_date",     s.t_date);
  ch |= FormBuilder::updateValue(root, "t_time",     s.t_time);
  ch |= FormBuilder::updateValue(root, "tick_ms",    s.tick_ms);
  // Defensive clamp — direct REST callers can bypass the slider's UI bounds.
  if (s.tick_ms < 10)   s.tick_ms = 10;
  if (s.tick_ms > 5000) s.tick_ms = 5000;
  return ch ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}

// ===== WS plain-state push (live tab) =====
// Each trend field arrives as an ARRAY of points keyed by the trend
// field's name; each point carries `timestamp` plus the line values.
void LightState::readSta(LightState& s, JsonObject& root) {
  root["t_text"]     = s.t_text;
  root["t_number"]   = s.t_number;
  root["t_slider"]   = s.t_slider;
  root["t_checkbox"] = s.t_checkbox;
  root["t_switch"]   = s.t_switch;
  root["t_button"]   = s.t_button;
  root["t_dropdown"] = s.t_dropdown;
  root["t_radio"]    = s.t_radio;
  root["t_textarea"] = s.t_textarea;

  root["chart_sin"]  = s.chart_sin;
  root["chart_cos"]  = s.chart_cos;
  root["uptime_ms"]  = s.uptime_ms;
  root["tick_ms"]    = s.tick_ms;

  root["t_datetime"] = s.t_datetime;
  root["t_date"]     = s.t_date;
  root["t_time"]     = s.t_time;

  auto emitChartPoint = [&](const char* key) {
    JsonArray arr = root.createNestedArray(key);
    JsonObject pt = arr.createNestedObject();
    pt["timestamp"] = s.chart_ts;
    pt["chart_sin"] = s.chart_sin;
    pt["chart_cos"] = s.chart_cos;
  };
  emitChartPoint("chart_line");
  emitChartPoint("chart_bar");

  // sin² + cos² ≡ 1 keeps the pie always full; the two wedges sweep
  // between 100% sin² and 100% cos² as phase advances — clean visual
  // proof both halves are live.
  {
    JsonArray arr = root.createNestedArray("chart_pie");
    JsonObject pt = arr.createNestedObject();
    pt["timestamp"] = s.chart_ts;
    pt["sin_sq"]    = s.chart_sin * s.chart_sin;
    pt["cos_sq"]    = s.chart_cos * s.chart_cos;
  }

  // Live table — append-mode push. Only the LATEST tick's row goes out
  // every WS broadcast; frontend appends and trims to maxRows.
  if (!s.table_rows.empty()) {
    JsonArray tbl = root.createNestedArray("t_table");
    const auto& r = s.table_rows.back();
    JsonObject row = tbl.createNestedObject();
    row["idx"]       = r.idx;
    row["timestamp"] = r.ts;
    row["chart_sin"] = r.sin_v;
    row["chart_cos"] = r.cos_v;
  }

  // MQTT inbox — streaming push, one or many rows per tick depending
  // on burst rate. tableMode("upsert") on frontend dedups by topic.
  // Same pattern MqttSettingsService uses for its sniffer table —
  // proves the consumer pattern works end-to-end inside an
  // application service, not just the framework module.
  if (!s.mqtt_pending.empty()) {
    JsonArray mq = root.createNestedArray("mqtt_inbox");
    for (const auto& t : s.mqtt_pending) {
      for (const auto& e : s.mqtt_inbox) {
        if (e.topic != t) continue;
        JsonObject row = mq.createNestedObject();
        row["topic"]      = e.topic;
        row["value"]      = e.value;
        row["count"]      = e.count;
        row["lastSeenAt"] = e.lastSeenAt_s;
        break;
      }
    }
    s.mqtt_pending.clear();
  }

  // Attached-values mirror — full replace each tick. List is small
  // (≤16 rows by MqttSettings::MQTT_ATTACHED_TOPICS_MAX) so we can
  // afford a complete dump without blowing the WS frame, and it
  // saves us the per-row "did this change since last tick" logic
  // that streaming/upsert would need. Empty list emits an empty
  // array — frontend's replace-mode TableField clears accordingly.
  JsonArray av = root.createNestedArray("attached_values");
  for (const auto& e : s.attached_view) {
    JsonObject row = av.createNestedObject();
    row["topic"] = e.topic;
    row["value"] = e.value;
  }
}

StateUpdateResult LightState::updateSta(JsonObject& root, LightState& s) {
  return updFs(root, s);
}

// ===== REST form schema + current values =====
// Mixed AF::R / AF::RW exercises the full permission matrix:
//   Settings tab     — editable + one read-only indicator (uptime)
//   Status tab       — writable + read-only echoes + trend charts
//   Integrations tab — proof of consumer-pattern wiring (Telegram +
//                       MQTT inbox + operator-attached topics + values)
void LightState::read(LightState& s, JsonObject& root) {
  // ---- Settings tab — editable widgets (AF::RW) + one AF::R indicator ----
  JsonArray set = FormBuilder::createForm(root, "settings", "FormBuilder widgets test bench");

  FormBuilder::addMessageField(set, "m_info",
      "This Settings tab exercises every FormBuilder widget type. "
      "Save applies to persisted state; Status tab shows WS-live echoes.",
      level("info"), icon("Info"));
  FormBuilder::addMessageField(set, "m_warn",
      "Action buttons at the bottom fire real backend calls — Restart and "
      "Factory Reset will reboot the device.",
      level("warning"), icon("Warning"));

  FormBuilder::addTextField    (set, "t_text",     AF::RW, s.t_text.c_str(),
                                icon("TextFields"));
  FormBuilder::addNumberField  (set, "t_number",   AF::RW, s.t_number,
                                minVal(-1000), maxVal(1000), format("0.00"),
                                icon("Thermostat"));
  FormBuilder::addSliderField  (set, "t_slider",   AF::RW, s.t_slider,
                                minVal(0), maxVal(100), step(1),
                                icon("Tune"));
  FormBuilder::addCheckboxField(set, "t_checkbox", AF::RW, s.t_checkbox,
                                icon("CheckBox"));
  FormBuilder::addSwitchField  (set, "t_switch",   AF::RW, s.t_switch,
                                icon("ToggleOn"));
  FormBuilder::addButtonField  (set, "t_button",   AF::RW, s.t_button,
                                placeholder("Trigger"), icon("PlayArrow"));
  FormBuilder::addDropdownField(set, "t_dropdown", AF::RW, s.t_dropdown,
                                opt("Alpha", 0), opt("Beta", 1), opt("Gamma", 2),
                                icon("ArrowDropDown"));
  FormBuilder::addRadioField   (set, "t_radio",    AF::RW, s.t_radio,
                                opt("Red", 0), opt("Green", 1), opt("Blue", 2),
                                icon("RadioButtonChecked"));
  FormBuilder::addTextareaField(set, "t_textarea", AF::RW, s.t_textarea,
                                icon("Notes"));
  FormBuilder::addFilesField   (set, "t_files",    AF::RW, "/rest/fs", "/flash", nullptr,
                                icon("Folder"));
  FormBuilder::addUploadField  (set, "t_upload",   AF::RW, "/rest/fs/upload", nullptr,
                                icon("Upload"), color("warning"));
  FormBuilder::addNumberField  (set, "uptime_ms",  AF::R,  (double)s.uptime_ms, format("0"),
                                icon("Timer"));

  // tick_ms — drag the slider to stress-test broadcast throughput.
  // Lower values = higher frequency. Persists through ConfigDelegate.
  FormBuilder::addSliderField  (set, "tick_ms",    AF::RW, (double)s.tick_ms,
                                minVal(10), maxVal(1000), step(10),
                                icon("Speed"));

  FormBuilder::addDateTimeField(set, "t_datetime", AF::RW, s.t_datetime,
                                dateTime(), icon("Schedule"));
  FormBuilder::addDateTimeField(set, "t_date",     AF::RW, s.t_date,
                                dateOnly(), icon("CalendarToday"));
  FormBuilder::addDateTimeField(set, "t_time",     AF::RW, s.t_time,
                                timeOnly(), icon("AccessTime"));

  FormBuilder::addActionField(set, "a_reload", nullptr, AF::RW, actionRef("light.reload"));
  FormBuilder::addActionField(set, "a_reset",  nullptr, AF::RW, actionRef("light.reset"));
  FormBuilder::addActionField(set, "a_ping",   nullptr, AF::RW, actionRef("light.ping"));
  FormBuilder::addActionField(set, "a_sysRestart",      nullptr, AF::RW, actionRef("system.restart"));
  FormBuilder::addActionField(set, "a_sysFactoryReset", nullptr, AF::RW, actionRef("system.factoryReset"));

  // ---- Status tab — live echoes (mix of AF::R and AF::RW) + 3 charts ----
  JsonArray sta = FormBuilder::createForm(root, "status", "Live state echo + sin/cos charts");
  FormBuilder::addTextField    (sta, "t_text",     AF::RW, s.t_text.c_str(),
                                icon("TextFields"));
  FormBuilder::addNumberField  (sta, "t_number",   AF::RW, s.t_number, format("0.00"),
                                icon("Thermostat"));
  FormBuilder::addSliderField  (sta, "t_slider",   AF::RW, s.t_slider,
                                minVal(0), maxVal(100), step(1), icon("Speed"));
  FormBuilder::addSliderField  (sta, "tick_ms",    AF::RW, (double)s.tick_ms,
                                minVal(10), maxVal(1000), step(10),
                                icon("Speed"));
  FormBuilder::addSwitchField  (sta, "t_switch",   AF::RW, s.t_switch,
                                icon("Lightbulb"));
  FormBuilder::addTextareaField(sta, "t_textarea", AF::RW, s.t_textarea,
                                icon("Notes"));

  FormBuilder::addCheckboxField(sta, "t_checkbox", AF::R,  s.t_checkbox,
                                icon("CheckBox"));
  FormBuilder::addDropdownField(sta, "t_dropdown", AF::R,  s.t_dropdown,
                                opt("Alpha", 0), opt("Beta", 1), opt("Gamma", 2),
                                icon("ArrowDropDown"));
  FormBuilder::addRadioField   (sta, "t_radio",    AF::R,  s.t_radio,
                                opt("Red", 0), opt("Green", 1), opt("Blue", 2),
                                icon("RadioButtonChecked"));

  FormBuilder::addNumberField  (sta, "chart_sin",  AF::R,  s.chart_sin, format("0.000"),
                                icon("Sensors"));
  FormBuilder::addNumberField  (sta, "chart_cos",  AF::R,  s.chart_cos, format("0.000"),
                                icon("Sensors"));

  FormBuilder::addTrendField(sta, "chart_line", AF::R,
      mode("lineChart"),
      xAxis("timestamp"),
      lines(line("chart_sin", "#e53935", "monotone"),
            line("chart_cos", "#1e88e5", "monotone")),
      legend(true), tooltip(true), trendMaxPoints(120),
      icon("ShowChart"));

  FormBuilder::addTrendField(sta, "chart_bar", AF::R,
      mode("barChart"),
      lines(line("chart_sin", "#43a047", "monotone"),
            line("chart_cos", "#fb8c00", "monotone")),
      legend(true), tooltip(true), trendMaxPoints(30),
      icon("BarChart"));

  FormBuilder::addTrendField(sta, "chart_pie", AF::R,
      mode("pieChart"),
      lines(line("sin_sq", "#8e24aa", "monotone"),
            line("cos_sq", "#00897b", "monotone")),
      legend(true), tooltip(true), trendMaxPoints(60),
      icon("PieChart"));

  FormBuilder::addTableField(sta, "t_table", AF::R,
      col("idx",       "#",     "number"),
      col("timestamp", "Time",  "number"),
      col("chart_sin", "sin",   "number", format("0.000")),
      col("chart_cos", "cos",   "number", format("0.000")),
      tableMode("append"), maxRows(50),
      icon("TableChart"));

  // ── INTEGRATIONS — proof the consumer side of the messaging
  // providers wires through into application code ──
  JsonArray integ = FormBuilder::createForm(
      root, "integrations", "Messaging integrations consumed by Light");

  FormBuilder::addMessageField(integ, "m_integrations",
      "These cards show how LightStateService consumes Telegram + MQTT "
      "via their subscription handles. Telegram block: outbound stats "
      "(messages we sent during the demo). MQTT inbox: live key-value "
      "table of every topic this service is hearing — populated by "
      "the onMessage('#') handler stuffing values into a std::map. "
      "Operator-attached topics from the MQTT tab flow in here too.",
      level("info"), icon("Cable"));

  // Telegram subscription stats — readback of what the bot module
  // exposed via TelegramSubscription::stats() at last form fetch.
  // Static snapshot per REST GET; refresh by reopening the tab.
  FormBuilder::addTextField(integ, "tg_service", AF::R, "lightControl",
                            label("Telegram service tag"), icon("Telegram"));

  // MQTT inbox table — reflects _values map mirrored into mqtt_inbox.
  // tableMode("upsert") with first-column key (`topic`) merges rows
  // delivered one-or-more-per-WS-tick from readSta. Cap 32 in heap +
  // maxRows(32) in frontend keeps memory bounded. We seed the
  // current snapshot on form GET so a tab-refresh shows accumulated
  // values instead of an empty grid awaiting the next WS push.
  JsonObject inbox = FormBuilder::addTableField(integ, "mqtt_inbox", AF::R,
      col("topic",      "Topic",     "text"),
      col("value",      "Value",     "text"),
      col("count",      "Count",     "number", format("0")),
      col("lastSeenAt", "Last (s)",  "number", format("0")),
      tableMode("upsert"), maxRows(32),
      icon("Inbox"),
      label("MQTT inbox (consumed via onMessage('#'))"));
  JsonArray inboxRows = inbox["mqtt_inbox"].as<JsonArray>();
  for (const auto& e : s.mqtt_inbox) {
    JsonObject row = inboxRows.createNestedObject();
    row["topic"]      = e.topic;
    row["value"]      = e.value;
    row["count"]      = e.count;
    row["lastSeenAt"] = e.lastSeenAt_s;
  }

  // Attached values table — JOIN of two consumer-pattern halves:
  //   left  = `_mqtt->attachedTopics()` — operator-curated selection
  //           from MQTT tab (persists across reboot)
  //   right = `_values` map populated by THIS service's onMessage('#')
  // For each operator-pinned topic we show the last payload Light
  // saw on the wire. Empty value = topic attached but no message
  // received yet (broker has nothing retained, or the publisher
  // hasn't sent since boot). Read-only seed — the table updates on
  // every REST GET / refetchForm; live tick is the broader
  // mqtt_inbox above so we don't fight upsert semantics here.
  JsonObject attVal = FormBuilder::addTableField(integ, "attached_values", AF::R,
      col("topic",  "Attached topic", "text"),
      col("value",  "Last value",     "text"),
      tableMode("replace"), maxRows(16),
      icon("PushPin"),
      label("Operator-attached topics + last value"));
  // Pull the topic list from `s.attached_view` — mirrored into state
  // by loop() once a tick (see the join logic there). buildForm is
  // static (ConfigDelegate's secret-key probe runs it on a default-
  // constructed state too), so we can't reach `_mqtt` / `_values`
  // here directly. Mirror-into-state keeps the static contract.
  JsonArray attRows = attVal["attached_values"].as<JsonArray>();
  for (const auto& e : s.attached_view) {
    JsonObject row = attRows.createNestedObject();
    row["topic"] = e.topic;
    row["value"] = e.value;
  }
}

StateUpdateResult LightState::update(JsonObject& root, LightState& s) {
  return updFs(root, s);
}
