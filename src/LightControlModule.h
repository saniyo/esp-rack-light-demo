#pragma once
#ifndef LIGHT_CONTROL_MODULE_H
#define LIGHT_CONTROL_MODULE_H

// LightControlModule — application module for esp-rack-light-demo.
// Owns the LightStateService that drives the demo's chart/widget UI.
//
// Lifecycle priority 60: lands AFTER the ESPRack base modules (wifi=10,
// filesystem=20, ntp=30, mqtt/ota/auto-update/telegram=40, sysstatus/
// wsdiag=80, configmgr-ui/web-endpoints=85) but BEFORE the action
// modules (restart/factory-reset=90). Doesn't depend on any of them
// at install time — only ConfigManager + WebManager — but ordering
// keeps the menu sensible (Light tab appears before the System shell
// in the manifest's natural insertion order).
//
// FT_PROJECT gates whether this module participates: if the consumer
// flips FT_PROJECT=0 the install does nothing, the WebManager never
// sees the lightState feature and the React UI hides the Light tab.

#include <Module.h>
#include <App.h>
#include <Features.h>
#include "LightStateService.h"
#include <memory>

class LightControlModule : public ESPRack::Module {
 public:
  void describe(ESPRack::ModuleDescriptor& d) override {
    d.id       = "lightControl";
    d.version  = "1.0.0";
    d.priority = 60;   // installs AFTER telegram(40), so app->telegram() is non-null
  }

  void onInstall(ESPRack::ModuleContext& ctx) override {
#if FT_ENABLED(FT_PROJECT)
    // Pull the telegram provider via App — TelegramModule late-binds it
    // during its own onInstall (priority 40), and Builder topo-sorts so
    // priority 60 modules see the populated pointer here. Null-safe:
    // if TelegramModule isn't installed at all, LightStateService just
    // doesn't subscribe.
    ITelegramProvider* tg = ctx.app ? ctx.app->telegram() : nullptr;
    svc_.reset(new LightStateService(ctx.cfgMgr, ctx.web, tg));
    if (ctx.deviceVersion) svc_->setVersion(ctx.deviceVersion);
#else
    (void)ctx;
#endif
  }

  void onBegin() override { if (svc_) svc_->begin(); }
  void onLoop()  override { if (svc_) svc_->loop();  }

 private:
  std::unique_ptr<LightStateService> svc_;
};

#endif  // LIGHT_CONTROL_MODULE_H
