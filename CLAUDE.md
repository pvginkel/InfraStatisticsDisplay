# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP-IDF 5.5.3 firmware (C++, ESP32-S3) for a Waveshare 7.5" e-paper display that polls the Infra Statistics
service and renders homelab metrics: Kubernetes node health, container/pod counts, container starts, recent
Jenkins builds, and failed builds/jobs. It is an esp-mdm device: network, MQTT, OTA, logging and provisioning all
come from the shared `esp-libs` components, and the device is managed over the air by IoT Support.

The backend it polls is the `infra-statistics` image in the DockerImages repo, checked out beside this one as
`../DockerImages/infra-statistics` (see "Backend contract" below). Changes to the JSON shape have to land on both
sides.

## Commands

The build needs the shared component library checked out **beside** this repo as `../esp-libs`
(`main/idf_component.yml` uses `../../esp-libs/<component>` path dependencies and the top-level `CMakeLists.txt`
pulls in their `sdkconfig.defaults`). CI clones it there; the KubeCoder environment mounts it read-only at
`/work/esp-libs`. Never edit it from here.

```sh
kc project build     # cexec esp-idf idf.py build  (the curated entry point; use this)
kc project lint      # arch-validate (aac-tools) over docs/architecture/*.yaml, then clang-format --dry-run over main/
kc project info      # lists what is wired

cexec esp-idf idf.py fullclean          # forces a dependency re-resolve on the next build
cexec esp-idf idf.py menuconfig         # Kconfig UI; app options are in main/Kconfig.projbuild
cexec native clang-format -i main/StatsUI.cpp   # format one file (style: main/.clang-format, Google/4-space/120 cols)
```

- `cexec esp-idf` already runs inside the IDF activation wrapper; do not prefix with `/opt/esp/entrypoint.sh` the way
  the Jenkinsfile does. Every `cexec esp-idf` call prints a multi-line toolchain banner first (its last stdout line is
  literally `idf.py build`); it is not an error.
- **There is no test suite.** `kc project test` is intentionally unset. Behaviour is verified by flashing a device,
  which cannot be done from this environment. Flashing, `idf.py monitor` and the Windows LVGL simulator are the
  operator's workstation flow (see `.vscode/settings.json`, which is full of Windows paths on purpose).
- `kc project lint` step one is a network call to `architecture.webathome.org`. Exit 2 means the service is down,
  exit 1 means the YAML is invalid. Only exit 1 is a repo problem.
- Regenerating fonts is `pnpm run generate-fonts` in `tools/`, driven by `tools/generate-fonts.json`. In-pod:
  `cexec frontend sh -c 'cd tools && pnpm install --frozen-lockfile && pnpm run generate-fonts'`; on Windows,
  `tools/generate-fonts.bat`. The `lv_font_*.c` files are its committed output and are excluded from clang-format.
  Their `Opts:` header records the `-o` path with the host's separator, so a Linux run rewrites that one comment
  line (`..\main` → `../main`) in every file even when the glyph tables are unchanged.
- `sdkconfig`, `dependencies.lock`, `managed_components/` and `build/` are gitignored. Only `sdkconfig.defaults` is
  the source of truth for config; a fresh build regenerates the rest.
- Deployment is CI-only: the Jenkinsfile builds and then runs `scripts/upload.sh` against IoT Support's pipeline
  upload endpoint. `PROJECT_VER` is `git rev-parse --short HEAD`, so the firmware version is the commit hash.

## Architecture

### Two layers: esp-mdm platform below, this repo's UI above

`Application` (main/Application.cpp) extends `ApplicationBase` from `esp-libs/esp-mdm`. The base class owns WiFi,
IoT Support provisioning (device config is read from NVS, written by IoT Support, so no addresses are hardcoded
except the stats endpoint), Keycloak M2M token, MQTT connection with Home Assistant discovery, OTA, log shipping and
core-dump upload. This repo only overrides the lifecycle hooks:

| Hook | What this repo does |
| --- | --- |
| `do_begin()` | Spawns the UI task pinned to core 1, wires MQTT discovery/connect callbacks |
| `do_network_connection_failed()` | Switches `LoadingUI` to its error state (which arms a restart timer) |
| `do_ready()` | Deletes `LoadingUI`, creates `StatsUI` |
| `do_process()` | Called every loop iteration; drives `Device::process()` and `StatsUI::update()` |

`main.cpp` is trivial: construct `Device`, construct `Application`, call `begin()` on both, then idle forever.

### Threading model

All LVGL work happens on **one task**, `Application::run` (core 1). Its loop is: `process()` (the base-class
state machine plus `do_process`), then `lv_timer_handler()`, then sleep to a ~10 ms cadence and `lv_tick_inc`.
`Device::begin()` additionally installs a 2 ms `esp_timer` that calls `lv_tick_inc`. Do not call LVGL from MQTT or
HTTP callbacks; hand the work to the loop instead. The HTTP fetch in `StatsUI::update_stats()` runs synchronously on
this task, which is why the schedule below deliberately fires 10 s early.

### UI classes

`LvglUI` (main/LvglUI.h) is the abstract screen base: `render()` cleans `lv_scr_act()`, re-inits the theme, then
calls the subclass's `do_render(parent)`. Screens are rebuilt from scratch on every render rather than mutated.
`LoadingUI` shows the startup title or an error; `StatsUI` is the main dashboard, laid out entirely with LVGL grid
descriptors (`static lv_coord_t ... LV_GRID_TEMPLATE_LAST` arrays) and the helpers in `lv_support.h`.

`StatsUI::do_update()` computes the next poll time aligned to `CONFIG_INFRA_STATISTICS_UPDATE_INTERVAL`
boundaries from the top of the hour (default 1800 s) minus 10 s, downloads `CONFIG_INFRA_STATISTICS_ENDPOINT`
(max 128 KB), parses it with `StatsDto::from_json` (cJSON, strict field-type checks, fails the whole parse on any
bad item), and calls `render()`. A failed download or parse keeps the previous screen.

### Backend contract (`../DockerImages/infra-statistics`)

The endpoint is a small Flask service: `app/main.py` composes the document, `app/myjenkinsapi.py` and
`app/mykubernetesapi.py` fetch from Jenkins and the Kubernetes API. `GET /stats?jobs=N` returns one JSON object,
and its Python dataclasses are the authoritative shape that `StatsDto::from_json` must mirror. The firmware asks
for `jobs=8` (baked into `CONFIG_INFRA_STATISTICS_ENDPOINT`); `jobs` bounds the length of each list below.

| Key | Items | Notes |
| --- | --- | --- |
| `last_builds`, `last_failed_builds` | `{name, number, execution, status}` | `status` ∈ IN_PROGRESS, ABORTED, FAILURE, NOT_BUILT, SUCCESS, UNSTABLE. An unknown value fails the whole parse, so a new status server-side needs `parse_jenkins_build_status` updated too. Failed builds are limited to the last 14 days. |
| `nodes` | `{name, created, ready, cordoned, allocated_pods, allocated_containers, cpu_capacity, cpu_usage, memory_capacity, memory_usage}` | CPU in nanocores, memory in KiB. The UI only shows usage/capacity percentages, so units cancel. |
| `last_failed_jobs` | `{name, namespace, created, completed, succeeded, failed}` | `namespace` lands in `KubernetesJobDto::ns`; `completed` may be `null`. Also 14-day bounded. |
| `container_starts` | `{day, week}` | Counted by a pod watch in the service and persisted to `/data`, not derived from the cluster on request. |

Top-level keys are optional to the firmware, but every nested field is required and type-checked: one missing or
mistyped field rejects the document and the previous screen stays. The UI shows at most 8 rows per column; the
failures column merges `last_failed_builds` and `last_failed_jobs` by time before truncating.

**Timestamps are pre-shifted.** The service emits `execution`, `created` and `completed` as UTC epoch **plus the
`TIMEZONE` offset** (Europe/Amsterdam in deployment). The device never sets `TZ`, so its `localtime` is UTC and the
shifted values render as local wall-clock time. Adding a timezone to the firmware, or making the backend emit true
UTC, breaks the display unless the other side changes with it.

### Display path

`Device` owns a `WaveshareEPaper7P5InV2alt` (main/waveshare_epaper.cpp, adapted from ESPHome) and registers the
LVGL display driver in `full_refresh` mode with a single full-screen draw buffer in SPIRAM. LVGL is built with
`CONFIG_LV_COLOR_DEPTH_1`; `Device::flush_cb` packs those 1-bit pixels into the e-paper's byte-per-8-pixels frame
buffer and triggers a full panel update. Pin numbers live in `waveshare_epaper.h` as `EPD_*_PIN` macros (the
`DISPLAY_PIN_*` Kconfig entries are not what the driver reads).

### `LV_SIMULATOR`

The `#ifndef LV_SIMULATOR` guards throughout `main/` exist so the UI classes compile inside the Windows LVGL
simulator (`tools/windows_simulator`, an uninitialised submodule). Under the simulator there is no ESP-IDF, no
`Device`, and `StatsUI` exposes `get_stats()` so the host can inject a `StatsDto`. Keep new UI code inside these
guards where it touches ESP APIs.

### Conventions worth knowing

- Every `.cpp` starts with `#include "includes.h"` first (the `.clang-format` `IncludeCategories` rule enforces the
  ordering). `includes.h` pulls in the whole ESP-IDF/LVGL surface and `using namespace std`.
- `LOG_TAG(Name);` at file scope defines `TAG` for `ESP_LOGx`.
- User-visible strings and Font Awesome codepoints are macros in `main/Messages.h` (Dutch UI text). Icons come
  from the `lv_font_icons_*` fonts; adding a glyph means adding its codepoint to `tools/generate-fonts.json` and
  regenerating.
- `StatsDto` is non-copyable and non-movable by design; parse into an existing instance.
- `ESP_ERROR_ASSERT`, `ESP_ERROR_CHECK_JUMP`, `ESP_TIMER_MS/SECONDS` and `esp_http_download_string` are the local
  helpers in `main/support.h`; `strformat`, `Callback`, `Queue`, `NVSProperty` come from `esp-libs/esp-support`.
- `docs/architecture/architecture.yaml` is the Architecture-as-Code artifact for this repo, validated by CI's
  `Jenkinsfile.architecture` job and by `kc project lint`. Update it when the firmware gains or loses an external
  dependency.
