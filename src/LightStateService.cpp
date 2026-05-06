#include "LightStateService.h"

#include <math.h>

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
}

StateUpdateResult LightState::updateSta(JsonObject& root, LightState& s) {
  return updFs(root, s);
}

// ===== REST form schema + current values =====
// Mixed AF::R / AF::RW exercises the full permission matrix:
//   Settings tab — editable + one read-only indicator (uptime)
//   Status tab   — writable + read-only echoes + trend charts
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
}

StateUpdateResult LightState::update(JsonObject& root, LightState& s) {
  return updFs(root, s);
}

// ===== Service =====
LightStateService::LightStateService(ConfigManager* cfgMgr, WebManager* web)
    : StatefulService<LightState>(),
      _cfg(cfgMgr,
           "lightState",
           LIGHT_SETTINGS_FILE,
           4096,
           this,
           LightState::readFs,
           LightState::updFs,
           false /*autoSave*/),
      _web(web) {
  if (!web) return;

  WebFeatureSpec spec;
  spec.id         = "lightState";
  spec.title      = "Light Service";
  spec.component  = "LightControl";
  spec.menu.label = "Light";
  spec.menu.icon  = "Lightbulb";
  spec.menu.order = 50;
  spec.menu.auth  = WebAuthLevel::Authenticated;
  spec.auth       = WebAuthLevel::Authenticated;
  spec.restRead   = LIGHT_SETTINGS_PATH;
  spec.restUpdate = LIGHT_SETTINGS_PATH;
  spec.wsPath     = LIGHT_SETTINGS_SOCKET_PATH;

  WebTabSpec statusTab;
  statusTab.key      = "status";
  statusTab.title    = "Status";
  statusTab.restPath = LIGHT_SETTINGS_PATH;
  statusTab.postable = false;
  statusTab.live     = true;
  spec.tabs.push_back(statusTab);

  WebTabSpec settingsTab;
  settingsTab.key      = "settings";
  settingsTab.title    = "Settings";
  settingsTab.restPath = LIGHT_SETTINGS_PATH;
  settingsTab.postable = true;
  spec.tabs.push_back(settingsTab);

  // 16k REST buffer carries every widget type + 3 trend configs; WS
  // stays compact (plain-state payload only).
  _feature = web->registerFeature<LightState>(
      std::move(spec), this,
      LightState::read, LightState::update,
      LightState::readSta, LightState::updateSta,
      16384, 2048);

  WebActionSpec reload;
  reload.id              = "light.reload";
  reload.title           = "Reload From FS";
  reload.icon            = "Refresh";
  reload.color           = "secondary";
  reload.auth            = WebAuthLevel::Authenticated;
  reload.successMessage  = "Reloaded from config file";
  reload.handler = [this](AsyncWebServerRequest* r) {
    (void)this->_cfg.ensureLoaded();
    r->send(200, "application/json", "{\"ok\":true}");
  };
  web->registerAction(reload);

  WebActionSpec reset;
  reset.id              = "light.reset";
  reset.title           = "Reset To Defaults";
  reset.icon            = "SettingsBackupRestore";
  reset.color           = "warning";
  reset.auth            = WebAuthLevel::Admin;
  reset.confirm         = "Reset light settings to defaults? State in RAM will be wiped.";
  reset.successMessage  = "Settings reset to defaults";
  reset.handler = [this](AsyncWebServerRequest* r) {
    this->update([](LightState& st) {
      st = LightState{};
      return StateUpdateResult::CHANGED;
    }, "action.reset");
    r->send(200, "application/json", "{\"ok\":true}");
  };
  web->registerAction(reset);

  WebActionSpec ping;
  ping.id              = "light.ping";
  ping.title           = "Ping";
  ping.icon            = "NetworkPing";
  ping.color           = "info";
  ping.auth            = WebAuthLevel::Authenticated;
  ping.successMessage  = "pong";
  ping.handler = [](AsyncWebServerRequest* r) {
    r->send(200, "application/json", "{\"pong\":true}");
  };
  web->registerAction(ping);
}

void LightStateService::loop() {
  unsigned long now = millis();
  uint16_t interval = _state.tick_ms > 0 ? _state.tick_ms : 100;
  if (now - _lastTick < interval) return;
  _lastTick = now;

  // Fixed phase step per tick — faster ticks visibly speed up the wave.
  // 100 ms → 10 Hz (≈1 rad/s sweep), 10 ms → 100 Hz (10× faster).
  _phase += 0.10;
  if (_phase > 6.283185307179586) _phase -= 6.283185307179586;

  double s = sin(_phase);
  double c = cos(_phase);

  update([this, s, c, now](LightState& st) {
    st.chart_sin = s;
    st.chart_cos = c;
    st.chart_ts  = (uint32_t)now;
    st.uptime_ms = (uint32_t)now;

    LightState::TableRow r;
    r.idx   = st.table_next_idx++;
    r.ts    = (uint32_t)now;
    r.sin_v = s;
    r.cos_v = c;
    st.table_rows.push_back(r);
    if (st.table_rows.size() > 10) {
      st.table_rows.erase(st.table_rows.begin());
    }

    return StateUpdateResult::CHANGED;
  }, "tick");
}

void LightStateService::begin() {
  (void)_cfg.ensureLoaded();

  _cfg.subscribe([](const LightState& s, const String& origin) {
    Serial.printf("[LightState] changed origin=%s t_number=%.2f t_slider=%.0f t_switch=%d\n",
                  origin.c_str(), s.t_number, s.t_slider, (int)s.t_switch);
  });

  if (_feature) _feature->broadcastWs("boot");
}
