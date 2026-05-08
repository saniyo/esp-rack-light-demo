#include "LightStateService.h"

#include <math.h>
#include <ITelegramProvider.h>
#include <IMqttProvider.h>

// ===== MQTT inbox upsert (called from LightStateService onMessage handler) =====
// Mirrors the topic→last-value map into the `mqtt_inbox` state vector
// (UI surface) and queues the topic for streaming WS push. We update
// state via update() so addUpdateHandler fires + WS broadcast
// schedules naturally — same path the chart tick uses.
void LightStateService::upsertInbox(const String& topic, const String& payload) {
  String t = topic;
  String p = payload;
  if (p.length() > 64) { p.remove(64); p += "..."; }

  update([t, p](LightState& s) {
    bool changed = false;
    bool found   = false;
    for (auto& e : s.mqtt_inbox) {
      if (e.topic == t) {
        if (e.value != p) e.value = p;
        e.count++;
        e.lastSeenAt_s = (uint32_t)(millis() / 1000);
        found = true;
        changed = true;
        break;
      }
    }
    if (!found) {
      if (s.mqtt_inbox.size() >= 32) {
        const String evicted = s.mqtt_inbox.front().topic;
        s.mqtt_inbox.erase(s.mqtt_inbox.begin());
        for (auto it = s.mqtt_pending.begin(); it != s.mqtt_pending.end(); ) {
          if (*it == evicted) it = s.mqtt_pending.erase(it); else ++it;
        }
      }
      LightState::MqttInboxEntry n;
      n.topic        = t;
      n.value        = p;
      n.count        = 1;
      n.lastSeenAt_s = (uint32_t)(millis() / 1000);
      s.mqtt_inbox.push_back(std::move(n));
      changed = true;
    }
    bool pending = false;
    for (const auto& q : s.mqtt_pending) if (q == t) { pending = true; break; }
    if (!pending) {
      if (s.mqtt_pending.size() >= 16) s.mqtt_pending.erase(s.mqtt_pending.begin());
      s.mqtt_pending.push_back(t);
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }, "mqtt");
}

// ===== Service =====
LightStateService::LightStateService(ConfigManager* cfgMgr,
                                     WebManager* web,
                                     ITelegramProvider* telegram,
                                     IMqttProvider* mqtt)
    : StatefulService<LightState>(),
      _cfg(cfgMgr,
           "lightState",
           LIGHT_SETTINGS_FILE,
           4096,
           this,
           LightState::readFs,
           LightState::updFs,
           false /*autoSave*/),
      _web(web),
      _telegram(telegram),
      _mqtt(mqtt) {
  if (!web) return;

  WebFeatureSpec spec;
  spec.id         = "lightState";
  spec.title      = "Light Service";
  // Service-implementation version — distinct from the wrapping
  // LightControlModule's wrapper version. Bump independently when the
  // chart logic / table format / WS push shape evolves.
  spec.version    = "0.1.0";
  // DynamicSettings = library's generic FormBuilder renderer. Everything
  // about how this feature looks comes from FormBuilder calls inside
  // LightState::read()/readSta() — there is no per-service React file.
  spec.component  = "DynamicSettings";
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

  // Integrations tab — surfaces what other framework providers
  // (Telegram, MQTT) the service is consuming. LightState::read()
  // emits the matching `integrations` sub-form key. Live too — the
  // MQTT inbox + attached-values tables tick on each loop tick.
  WebTabSpec integrationsTab;
  integrationsTab.key      = "integrations";
  integrationsTab.title    = "Integrations";
  integrationsTab.restPath = LIGHT_SETTINGS_PATH;
  integrationsTab.postable = false;
  integrationsTab.live     = true;
  integrationsTab.order    = 15;
  spec.tabs.push_back(integrationsTab);

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

  // Telegram demo — once a minute, push the latest sin/cos sample to
  // show the subscription provider working end-to-end. .active()
  // gates on (handle valid) AND (mute flag on) AND (bot not
  // Disabled) — keeps the recent-activity log free of spam "skip:
  // bot disabled" lines when the operator's left the bot toggled
  // off, which on a 1-tick-per-minute cadence would otherwise
  // accumulate fast.
  if (_telegramSub.active() && (now - _telegramLastSendMs) >= 60000) {
    _telegramLastSendMs = now;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "sin = %.3f, cos = %.3f (uptime %lus)",
             s, c, (unsigned long)(now / 1000));
    _telegramSub.send(String(buf));
  }

  // MQTT demo — same cadence, two separate retained topics so an
  // operator running `mosquitto_sub -h <host> -t 'demo/light/#' -v`
  // sees both legs cleanly. .active() gates on (handle valid AND
  // enabled AND broker not Disabled) so the publishDropped counter
  // doesn't tick when MQTT is intentionally off.
  if (_mqttSub.active() && (now - _mqttLastPubMs) >= 60000) {
    _mqttLastPubMs = now;
    char buf[24];
    snprintf(buf, sizeof(buf), "%.4f", s);
    _mqttSub.publish("sin", buf);
    snprintf(buf, sizeof(buf), "%.4f", c);
    _mqttSub.publish("cos", buf);
  }

  // Refresh the attached-view snapshot. Joins the operator's MQTT
  // attached_topics list (provider-owned) with our local _values map
  // (filled by the sniffer onMessage handler). Done in loop() so the
  // form reader reads a consistent vector without crossing into
  // service fields from the static buildForm path. Cheap — at most
  // 16 string copies per tick, only when something changed.
  if (_mqtt) {
    const auto& wanted = _mqtt->attachedTopics();
    bool needsRebuild = wanted.size() != _state.attached_view.size();
    if (!needsRebuild) {
      for (size_t i = 0; i < wanted.size(); ++i) {
        if (wanted[i] != _state.attached_view[i].topic) { needsRebuild = true; break; }
      }
    }
    // Also rebuild if any value changed for an existing topic — picks
    // up new payloads received between ticks without a topic-list shift.
    if (!needsRebuild) {
      for (auto& v : _state.attached_view) {
        auto it = _values.find(v.topic);
        const String& latest = it != _values.end() ? it->second : String();
        if (v.value != latest) { needsRebuild = true; break; }
      }
    }
    if (needsRebuild) {
      update([this, &wanted](LightState& st) {
        st.attached_view.clear();
        st.attached_view.reserve(wanted.size());
        for (const auto& t : wanted) {
          LightState::AttachedViewEntry e;
          e.topic = t;
          auto it = _values.find(t);
          e.value = it != _values.end() ? it->second : String();
          st.attached_view.push_back(std::move(e));
        }
        return StateUpdateResult::CHANGED;
      }, "attached");
    }
  }
}

void LightStateService::begin() {
  (void)_cfg.ensureLoaded();

  _cfg.subscribe([](const LightState& s, const String& origin) {
    Serial.printf("[LightState] changed origin=%s t_number=%.2f t_slider=%.0f t_switch=%d\n",
                  origin.c_str(), s.t_number, s.t_slider, (int)s.t_switch);
  });

  // Subscribe to Telegram with our service tag + a conservative
  // 1-msg/min cap so the chat never gets flooded by the chart.
  // defaultTopicId left empty — we ride the bot's global default.
  // Operator can rebuild firmware with a specific topic if they
  // want sin/cos samples in a dedicated thread.
  if (_telegram) {
    TelegramSubscriptionConfig cfg;
    // Markdown-safe prefix — square brackets would trigger Telegram's
    // link-syntax `[text](url)` parser in parseMode=Markdown and the
    // whole message would silently fail to render.
    cfg.tagPrefix             = "Light: ";
    cfg.maxMessagesPerMinute  = 2;          // ourselves + headroom
    _telegramSub = _telegram->subscribe("lightControl", cfg);
  }

  // MQTT subscription — prefix routes our publishes under demo/light/.
  // Default QoS-0 retained so a fresh subscriber gets the latest
  // sample immediately; rate cap of 4 covers our 2 publishes/min plus
  // a little headroom for any future per-tick beacons.
  if (_mqtt) {
    MqttSubscriptionConfig cfg;
    cfg.defaultTopicPrefix     = "demo/light/";
    cfg.defaultQos             = 0;
    cfg.defaultRetain          = true;
    cfg.maxPublishesPerMinute  = 4;
    _mqttSub = _mqtt->subscribe("lightControl", cfg);

    // Echo command channel — round-trip test that the inbound
    // dispatch path works. Published payload arrives back as a
    // Telegram message if Telegram is up, otherwise just to Serial.
    _mqttSub.onMessage("cmd/echo", [this](const String& topic, const String& payload) {
      Serial.printf("[Light] MQTT echo on %s: %s\n", topic.c_str(), payload.c_str());
      if (_telegramSub.active()) {
        _telegramSub.send(String("echo: ") + payload);
      }
    });

    // Sniffer-equivalent: separate subscription with empty prefix so
    // we hear EVERY topic the broker delivers (including ones the
    // operator attached via the MQTT tab's UI). Each message gets
    // upserted into _values + the mqtt_inbox state mirror, which
    // surfaces in the Status tab "MQTT inbox" table. Demonstrates
    // the consumer pattern from earlier — handler stores in heap,
    // valueOf() reads anywhere.
    MqttSubscriptionConfig snifferCfg;
    snifferCfg.defaultTopicPrefix    = "";
    snifferCfg.maxPublishesPerMinute = 0;  // outbound unused on sniffer
    _mqttSniffer = _mqtt->subscribe("lightSniffer", snifferCfg);
    _mqttSniffer.onMessage("#", [this](const String& topic, const String& payload) {
      _values[topic] = payload;
      upsertInbox(topic, payload);
    });
  }

  if (_feature) _feature->broadcastWs("boot");
}
