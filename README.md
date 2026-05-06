# esp-rack-light-demo

> Reference consumer of the [**ESPRack**](https://github.com/saniyo/esp-rack)
> firmware framework. Builds a Wi-Fi-enabled RGB-light controller as a
> single application module, demonstrating the framework's plug-and-play
> module API.

**Status**: pre-v0.1 — see ESPRack
[`ROADMAP.md`](https://github.com/saniyo/esp-rack/blob/main/ROADMAP.md)
for the joint plan.

## What this repo proves

* **One application module** — `LightControlModule` — implements the
  full RGB-light feature set that previously lived as 200+ lines of
  glue inside `ESP-SES-LightService`'s monolithic `ESPReact`.
* **Composition root in `main.cpp`** — every framework feature is opted
  in via `Builder.install<>()`. Adding a new module is one line.
* **Standard ESPRack hooks** — config persistence + UI tab + WS
  broadcasts come from the framework; the demo only writes
  hardware-specific code (GPIO, colour-mixing, schedules).

## What it looks like

```cpp
#include <ESPRack.h>
#include <modules/wifi/WiFiModule.h>
#include <modules/security/SecurityModule.h>
#include "LightControlModule.h"

AsyncWebServer server(80);
std::unique_ptr<ESPRack::App> app;

void setup() {
  Serial.begin(115200);
  app = ESPRack::Builder(&server, "ESPRackDemo", "v0.1.0")
    .core()                                          // wifi/ap/security/...
    .install<LightControlModule>(/*pinR=*/14, /*pinG=*/27, /*pinB=*/26)
    .build();
  app->begin();
}

void loop() { app->loop(); }
```

## How to build (after Phase 1 lands)

```
pio run -e esp32-c6-devkitc-1 -t upload
pio device monitor -e esp32-c6-devkitc-1 -b 115200
```

`platformio.ini` pulls ESPRack via `lib_deps`:

```ini
[env:esp32-c6-devkitc-1]
platform = ...
framework = arduino
lib_deps =
  https://github.com/saniyo/esp-rack.git#v0.1.0
  AsyncTCP
  ESPAsyncWebServer
build_flags =
  -DESPRACK_SECURITY=1
  -DESPRACK_SECRETS_VAULT=1
  ${features.build_flags}
```

## Layout

```
esp-rack-light-demo/
├── platformio.ini             # lib_deps = github.com/saniyo/esp-rack
├── features.ini               # consumer-side ESPRACK_* flags
├── package.json               # (Phase 3+) @user/esprack-ui dep
│
├── src/
│   ├── main.cpp               # composition root
│   ├── LightControlModule.h
│   ├── LightControlModule.cpp
│   └── LightSettings.h        # state + readConfig/update/buildForm
│
├── interface/                 # (optional) consumer-side React UI
│   └── src/
│       ├── App.tsx            # imports primitives from @user/esprack-ui
│       ├── Plugins.tsx        # custom widget map for /lightControl tab
│       └── LightChart.tsx     # custom React widget (optional)
│
└── data/                      # (optional) PIO data partition
```

## License

MIT — see [`LICENSE`](LICENSE).
