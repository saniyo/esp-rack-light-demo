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
#include <Features.h>
#include "LightStateService.h"
#include <memory>

class LightControlModule : public ESPRack::Module {
 public:
  void describe(ESPRack::ModuleDescriptor& d) override {
    d.id       = "lightControl";
    d.version  = "1.0.0";
    d.priority = 60;
  }

  void onInstall(ESPRack::ModuleContext& ctx) override {
#if FT_ENABLED(FT_PROJECT)
    svc_.reset(new LightStateService(ctx.cfgMgr, ctx.web));
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
