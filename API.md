# UngulaCore (`lib`)

Portable C++17 core utilities for embedded firmware: a platform-abstracted
time/delay source, NVS-backed key-value preferences with CRC-checked
program slots, fixed-capacity queue, string helpers, CRC32, MCU
diagnostics, and a heap health monitor. ESP32 (ESP-IDF) is the fully
supported target; the portable subset (strings, queue, math, CRC,
time on host) builds for STM32/host with the bundled host backend.

Preferences layout is split by responsibility:

- `ungula/core/preferences/i_preferences.h` — platform-agnostic interface.
- `ungula/core/preferences/platforms/*` — platform implementations.
- `ungula/core/preferences/preferences.h` — single include facade for hosts.
- `ungula/core/preferences/tools/*` — reusable preference-backed utilities.

The library is header-mostly. One umbrella header pulls in the public
surface for Arduino discovery; individual headers can also be included
directly when only part of the surface is needed.

---

## LLM quick map
- **Own source minimum**: `C++17`.
- **Effective minimum for consumers**: `C++17`.
- **Dependency impact**: None (no declared internal dependencies).

Use this section first, then jump to the detailed API sections.

- **Umbrella header**: `#include <ungula/core.h>` (or `<ungula_core.h>`, a flat
  forwarder that only exists for Arduino-CLI symlink discovery). It pulls in
  `util/queue.h`, `util/string_types.h`, `util/string_utils.h`, `util/types.h`,
  `preferences/preferences.h`, `time/time.h`, `system/health_monitor.h` and
  `system/system_reboot.h` — **nothing else**. CRC32, PID, ChipInfo,
  `NvsConfigStore`, `ProgramStore` and `CounterStore` need their own include.
- **Portable persistence contract**: `#include <ungula/core/preferences/i_preferences.h>`.
- **Platform-selected preferences facade**: `#include <ungula/core/preferences/preferences.h>`.
- **Single CRC-protected config struct**: `#include <ungula/core/preferences/nvs_config_store.h>`.
- **Program/recipe slots utility**: `#include <ungula/core/preferences/tools/programs/program_store.h>`.
- **Persistent counters utility**: `#include <ungula/core/preferences/tools/counters/counter_store.h>`.
- **Time only**: `#include <ungula/core/time/time.h>`.
- **CRC32**: `#include <ungula/core/util/crc32.h>`.
- **PID**: `#include <ungula/core/control/pid.h>`.
- **MCU identity**: `#include <ungula/core/system/chip_info.h>`.

### Platform matrix

| Module | ESP32 | Host tests / desktop | Notes |
| --- | --- | --- | --- |
| `time/*` | Yes | Yes | Host backend: `std::chrono` + `std::thread` |
| `preferences/i_preferences.h` | Yes | Yes | Interface-only |
| `preferences/preferences.h` (`initStorage()`) | Yes | Yes | Declaration in facade; implementation picked at link time |
| `preferences/platforms/esp32_preferences.*` | Yes | No | Compiled when `ESP_PLATFORM`, `ARDUINO_ARCH_ESP32`, or `ESP32` is defined; aliased as `Preferences` via `preferences.h`. Provides ESP32 `initStorage()` (wraps `nvs_flash_init`) |
| `preferences/platforms/host_preferences.cpp` | No | Yes | No-op `initStorage()` for host tests / non-MCU builds |
| `preferences/tools/programs/program_store.h` | Yes | Yes | Requires injected `IPreferences` implementation |
| `preferences/tools/counters/counter_store.h` | Yes | Yes | Header-only; requires injected `IPreferences` implementation |
| `preferences/nvs_config_store.h` | Yes | Yes | Header-only; requires injected `IPreferences` implementation |
| `util/*` (`queue`, `crc32`, `string_*`, `types`) | Yes | Yes | Header-only utility layer |
| `system/health_monitor.*` | Yes | Yes | Host returns heap counters as `0` |
| `system/system_reboot.*` | Yes | No | Non-ESP build errors at compile time |
| `system/chip_info.*` | Yes | No | Implementation uses ESP-IDF headers |
| `control/pid.h` | Yes | Yes | Header-only PID controller |

### LLM subsystem checklists

#### Time (`ungula::core::time`)

- **Do**: include `ungula/core/time/time.h` (or `ungula/core.h`) and call only `millis/micros/delay*/delayUntil*/yield/now*`.
- **Do**: use `delayUntilMs` for periodic loops; use `yield()` in tight supervisory loops.
- **Do**: install `ITimeProvider` before treating `now()` as real wall clock;
  check `hasValidWallClock()` before rendering a date to a UI.
- **Don't**: include `time/platforms/*` directly — `time.h` dispatches.
- **Don't**: call `esp_timer_*`, `vTaskDelay`, or Arduino `millis()/delay()` directly from domain code.
- **Don't**: truncate any of these values to `uint32_t`; the whole API is `int64_t`.

#### Preferences (`ungula::core::preferences`)

- **Do**: call `initStorage()` exactly once at boot, **before** `Preferences::begin()`, `wifi`, `espnow`, `ble`, or any other subsystem that touches non-volatile storage. Skipping it produces `ESP_ERR_NVS_NOT_INITIALIZED` on ESP-IDF and a reboot loop.
- **Do**: include `ungula/core/preferences/preferences.h` in composition-root code.
- **Do**: depend on `IPreferences` in reusable/domain classes.
- **Do**: pair every `begin(ns)` with `end()`.
- **Do**: use `ProgramStore` (`tools/programs/program_store.h`) for recipe/profile slots.
- **Do**: use `NvsConfigStore` (`nvs_config_store.h`) for a single CRC-protected config struct.
- **Do**: use `CounterStore` (`tools/counters/counter_store.h`) for lifetime event tallies (runs, boots, cycles) — one increment per discrete event, never per loop iteration.
- **Don't**: include `preferences/platforms/*` in portable domain code.
- **Don't**: read/write the `programs` namespace by hand when using `ProgramStore`.
- **Don't**: share one `Preferences` instance across tasks.
- **Don't**: spell the platform class name (`Esp32Preferences`) in application code — use the `Preferences` alias so the code stays portable.

##### `bool initStorage()` (free function, namespace `ungula::core::preferences`)

```cpp
#include <ungula/core/preferences/preferences.h>

extern "C" void app_main()
{
    if (!ungula::core::preferences::initStorage()) {
        abort();    // storage backend refused even after erase-and-retry
    }
    // wifi::espnow_init(); transport.init(); Preferences::begin(...); ...
}
```

Backend-agnostic free function. ESP32 backend wraps `nvs_flash_init()` and
handles `ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND` by
erasing the partition and retrying once. Host backend is a no-op returning
`true` so the same source compiles and links for unit tests.

Call this function prior to `wifi`, `espnow`, `ble`, or any
`Preferences::begin()`.

#### System (`ungula::core::system`)

- **Do**: treat `HealthMonitor` as portable (ESP + host test stubs).
- **Do**: gate `SystemControl` and `queryChipInfo()` usage to ESP32 builds.
- **Don't**: assume reboot/chip-info code compiles on non-ESP targets.

#### Utilities (`ungula::core::util`)

- **Do**: use `Queue<T,N>` for bounded no-heap buffering inside a single task.
- **Don't**: use `Queue<T,N>` across an ISR boundary — no barriers, no locking.
- **Do**: use `crc32`/`crc32_byte` for wire/storage integrity checks.
- **Do**: use `string_t`/`string_view_t` and `str::*` helpers for text utilities.
- **Don't**: reimplement CRC/queue/string helpers already provided here.

#### Control (`ungula::core::control`)

- **Do**: apply your own `[min, max]` clamp on the PID output before adding base bias/speed.
- **Do**: call `reset()` after setpoint jumps, loop disable, or hard-stop events.
- **Do**: ensure `dt_s > 0` on every `update()` call — zero or negative dt produces undefined derivative.
- **Don't**: assume the output is bounded — anti-windup clamps the integral accumulator, not the final output.

### LLM fast callsheet

#### Time

| Goal | Call | Return / behavior |
| --- | --- | --- |
| monotonic ms/us | `tc::millis()`, `tc::micros()`, `tc::nowUs()` | `int64_t` monotonic ticks (`nowUs` is an alias for `micros`) |
| blocking wait | `tc::delayMs(ms)`, `tc::delayUs(us)`, `tc::delay(ms)` | `void`; non-positive input is no-op |
| drift-free periodic loop | `tc::delayUntilMs(ref, period)` / `delayUntilUs` | advances `ref` by period |
| cooperative handoff | `tc::yield()` | ESP32: one RTOS tick (`vTaskDelay(1)`); host: thread yield |
| wall clock (provider-aware) | `tc::now()`, `tc::nowUtc()`, `tc::nowLocal()` | falls back to local monotonic clock when provider invalid |
| is the wall clock real? | `tc::hasValidWallClock()` | `true` only when a provider is installed **and** `isValid()` |
| zone conversion | `tc::setTimezone(...)`, `tc::nowInTz(offsetSec)` | fixed-offset only (no DST logic) |
| sync offset clock | `setSyncTime(Us)`, `syncNow(Us)`, `syncOffset(Us)`, `isSynced`, `clearSync` | additive offset model |

#### Preferences

| Goal | Call | Return / behavior |
| --- | --- | --- |
| open namespace | `prefs.begin("ns")` | `bool` success |
| close namespace | `prefs.end()` | `void` |
| write values | `putString/putBytes/putUInt8/putUInt32` | `bool` success |
| read values | `getString/getBytes` | bytes read (`0` on missing/error) |
| typed reads | `getUInt8/getUInt32(key, default)` | default on missing/error |
| key management | `hasKey/remove/clear` | `bool` |

#### ProgramStore

| Goal | Call | Return / behavior |
| --- | --- | --- |
| initialize slots | `store.init(defaultName, createDefault)` | creates slot 0 when none valid |
| read slot | `store.getProgram(index)` | out-of-range clamps to slot 0 |
| save slot | `store.saveProgram(index, p)` | forces `valid=true`; persists immediately |
| delete slot | `store.deleteProgram(index)` | refuses to delete last valid slot |
| active slot | `getActiveIndex/setActiveIndex` | rejects invalid target slot |
| metadata | `getLastUsedIndex/setLastUsedIndex` | persisted in same namespace |
| capacity | `countValid/maxPrograms` | count / compile-time capacity |

#### CounterStore

| Goal | Call | Return / behavior |
| --- | --- | --- |
| read a tally | `counters.read("runs")` | `uint32_t`; `0` when never created |
| set / create | `counters.write("runs", n)` | `bool` success |
| count one event | `counters.increment("runs")` | new value; `0` on failure; saturates at `UINT32_MAX` |
| count by step | `counters.increment("meters", n)` | same, adding `n` |
| zero it | `counters.reset("runs")` | `bool`; key kept |
| created yet? | `counters.exists("runs")` | `bool` |
| name check | `CounterStore::isValidName(n)` | `bool`; 1..15 chars |

---

## Usage

### Use case: Drift-free periodic loop

```cpp
#include <ungula/core.h>

namespace tc = ungula::core::time;

void setup() {}

void loop() {
    auto ref = tc::millis();
    while (true) {
        // ... read sensors, push status ...
        tc::delayUntilMs(ref, 50);   // every 50 ms, no drift
    }
}
```

When to use this: any periodic task (sensor poll, heartbeat, control
loop). `delayUntilMs` advances `ref` by the period, so jitter from work
inside the loop does not accumulate.

### Use case: Microsecond-grade sleep

```cpp
#include <ungula/core/time/time.h>

ungula::core::time::delayUs(250);   // ~250 µs busy-wait on ESP32
```

When to use this: bit-banging, short hardware-timing windows. Prefer
`delayMs` for anything ≥ 1 ms — it yields to FreeRTOS.

### Use case: Wall-clock time via a pluggable provider

```cpp
#include <ungula/core/time/time.h>
#include <ungula/core/time/i_time_provider.h>

class NtpClock : public ungula::core::time::ITimeProvider {
    public:
        int64_t nowMs() const override { return epochMs_; }
        bool    isValid() const override { return synced_; }
        void    onSync(int64_t epochMs) { epochMs_ = epochMs; synced_ = true; }
    private:
        int64_t epochMs_ = 0;
        bool    synced_  = false;
};

static NtpClock clock;

void setup() {
    ungula::core::time::setTimeProvider(&clock);
    ungula::core::time::setTimezone(ungula::core::time::tz::Timezone::CET);
}

void printNow() {
    char buf[20];
    if (ungula::core::time::formatLocal(buf, sizeof(buf)) > 0) {
        // buf == "2026-04-26 14:32:01"
    }
}
```

When to use this: any code that needs a real wall-clock instant
(logging timestamps, scheduled tasks). Without a provider, `now()`
falls back to monotonic-since-boot.

### Use case: Persisted key-value preferences (ESP32)

```cpp
#include <ungula/core/preferences/preferences.h>

ungula::core::preferences::Preferences prefs;

void saveSsid(const char* ssid) {
    if (!prefs.begin("wifi")) return;
    prefs.putString("ssid", ssid);
    prefs.end();
}

bool readSsid(char* buf, size_t bufSize) {
    if (!prefs.begin("wifi")) return false;
    size_t n = prefs.getString("ssid", buf, bufSize);
    prefs.end();
    return n > 0;
}
```

When to use this: configuration that must survive reboot. Namespace
names are limited to 15 characters by NVS.

### Use case: Versioned program/recipe slots with CRC

```cpp
#include <ungula/core/preferences/preferences.h>
#include <ungula/core/preferences/tools/programs/program_store.h>

#include <cstdio>

struct Recipe {
    char  name[32];
    int   speedRpm;
    float targetC;
    bool  valid;
};

static Recipe defaultRecipe(const char* name) {
    Recipe r{};
    std::snprintf(r.name, sizeof(r.name), "%s", name);
    r.speedRpm = 1000;
    r.targetC  = 25.0f;
    r.valid    = true;
    return r;
}

ungula::core::preferences::Preferences           prefs;
ungula::core::preferences::programs::ProgramStore<Recipe, 10>   store(prefs);

void setup() {
    store.init("DEFAULT", &defaultRecipe);

    Recipe r = store.getProgram(store.getActiveIndex());
    r.speedRpm = 1500;
    store.saveProgram(store.getActiveIndex(), r);
}
```

When to use this: a fixed-size table of user recipes/profiles where
each slot must be self-checking (corruption survives reboots
otherwise). CRC32 is computed across `sizeof(ProgramT)` and rejects
any slot that fails verification on load.

### Use case: Single CRC-protected config struct

```cpp
#include <ungula/core/preferences/nvs_config_store.h>

struct DeviceConfig {
    float setpoint = 25.0f;
    float kp = 1.0f;
    float ki = 0.1f;
    float kd = 0.05f;
    uint32_t flags = 0;
};

static_assert(std::is_trivially_copyable<DeviceConfig>::value);

NvsConfigStore<DeviceConfig> cfg(prefs, "dev", "cfg");

void loadConfig() {
    ConfigLoadStatus status;
    DeviceConfig loaded = cfg.load({}, &status);
    if (status == ConfigLoadStatus::Recovered) {
        log_warn("Config was corrupted, reset to defaults");
    }
    applyConfig(loaded);
}

void saveConfig(const DeviceConfig& c) {
    cfg.save(c);
}
```

When to use this: one device-wide config struct (not a table of slots).
CRC integrity check detects corruption; `Recovered` status triggers an
automatic rewrite of defaults so the device self-heals on boot.

### Use case: Persistent execution counter

```cpp
#include <ungula/core/preferences/tools/counters/counter_store.h>

namespace counters = ungula::core::preferences::counters;

counters::CounterStore machineCounters(prefs);   // namespace "counters"

void onBoot() {
    machineCounters.increment("boots");
}

void onProgramStarted() {
    const uint32_t n = machineCounters.increment("runs");   // new value
    log_info("run #%u", (unsigned)n);
}

uint32_t totalRuns() {
    return machineCounters.read("runs");          // 0 when never created
}

void onFactoryReset() {
    machineCounters.reset("runs");                // 0, key kept
}
```

When to use this: lifetime tallies of discrete events — programs executed,
boots, cycles per component, faults seen. One key per counter, so writing
one never rewrites the others.

When NOT to use it: anything counted per loop iteration or per tick. One
storage write per increment; keep those in RAM and persist the total once
when the run ends.

### Use case: Fixed-capacity queue (no heap)

```cpp
#include <ungula/core/util/queue.h>

ungula::core::util::Queue<int, 16> q;

void onSample(int sample) {
    q.push(sample);          // false if full — caller decides
}

void drain() {
    int v;
    while (q.pop(v)) {
        // process
    }
}
```

When to use this: a bounded buffer inside one task where heap allocation is
forbidden. **Not** for ISR-to-task handoff — `Queue` has no memory barriers
and no critical sections. Use a FreeRTOS queue for that.

### Use case: String helpers

```cpp
#include <ungula/core/util/string_utils.h>

using ungula::core::util::string_t;
using namespace ungula::core::util::str;

string_t s = "  Hello, World  ";
trim(s);                                     // "Hello, World"
to_lower(s);                                 // "hello, world"
int n = replaceAll(s, "world", "ungula");    // 1
auto parts = tokenizeByDelimiter(s, ',');    // ["hello", " ungula"]
```

### Use case: CRC32 of a buffer

```cpp
#include <ungula/core/util/crc32.h>

uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
uint32_t crc = ungula::core::util::crc32(payload, sizeof(payload));
```

### Use case: Reboot the MCU

```cpp
#include <ungula/core/system/system_reboot.h>

ungula::core::system::SystemControl::reboot();              // immediate
ungula::core::system::SystemControl::rebootAfterMs(2000);   // give logs time to flush
```

### Use case: Heap-leak watchdog

```cpp
#include <ungula/core/system/health_monitor.h>

static ungula::core::system::HealthMonitor health;

void loop() {
    ungula::core::system::HealthSample s;
    if (health.sample(60000, s)) {
        // emit s.free_heap, s.min_free_heap, s.delta, s.uptime_ms
    }
}
```

When to use this: long-running firmware. A monotonically falling
`free_heap` is the only early signal of an allocator leak.

### Use case: Single-loop PID control

```cpp
#include <ungula/core/control/pid.h>

ungula::core::control::PidConfig cfg = { .kp = 1.2f, .ki = 0.05f, .kd = 0.3f, .i_max = 40.0f };
ungula::core::control::Pid motorPid(cfg);

void setTarget(float rpm) {
    targetRpm = rpm;
    motorPid.reset();  // clear history on setpoint change
}

void controlLoop(float dt_s) {
    float error = targetRpm - currentRpm;
    float output = motorPid.update(error, dt_s);

    float drive = std::clamp(output + baseDrive, minDrive, maxDrive);
    motor.setDrive(drive);
}
```

When to use this: motor speed, temperature, or any continuous variable
that needs a feedback loop. The integral accumulator is
anti-windup-clamped internally so a saturated output does not
accumulate error indefinitely.

### Use case: Identify the MCU at boot

```cpp
#include <ungula/core/system/chip_info.h>

void printBootBanner() {
    ungula::core::system::ChipInfo info = ungula::core::system::queryChipInfo();
    // info.model, info.sdkVersion, info.cores, info.hasWifi, ...
}
```

---

## Public types

| Type | Header | Purpose |
| ---- | ------ | ------- |
| `ungula::core::time` (namespace) | `ungula/core/time/time.h` | Free-function time/delay API |
| `ungula::core::time::ITimeProvider` | `ungula/core/time/i_time_provider.h` | Pluggable wall-clock source |
| `ungula::core::time::tz::Timezone` (enum) | `ungula/core/time/timezones.h` | Named UTC offset codes |
| `ungula::core::preferences::IPreferences` | `ungula/core/preferences/i_preferences.h` | Abstract NVS interface |
| `ungula::core::preferences::Preferences` | `ungula/core/preferences/preferences.h` | Platform-selected alias (concrete impl picked at compile time) |
| `ungula::core::preferences::Esp32Preferences` | `ungula/core/preferences/platforms/esp32_preferences.h` | ESP-IDF NVS implementation (internal — use the `Preferences` alias from app code) |
| `ungula::core::preferences::programs::ProgramStore<T, N>` | `ungula/core/preferences/tools/programs/program_store.h` | CRC-checked recipe slot table |
| `ungula::core::preferences::NvsConfigStore<T>` | `ungula/core/preferences/nvs_config_store.h` | CRC-protected single-config persistence |
| `ungula::core::preferences::ConfigLoadStatus` | `ungula/core/preferences/nvs_config_store.h` | Load result: Loaded / Defaulted / Recovered |
| `ungula::core::preferences::counters::CounterStore` | `ungula/core/preferences/tools/counters/counter_store.h` | Named persistent `uint32_t` counters (runs, boots, cycles) |
| `ungula::core::util::Queue<T, Capacity>` | `ungula/core/util/queue.h` | Fixed-capacity circular queue |
| `ungula::core::system::SystemControl` | `ungula/core/system/system_reboot.h` | Reboot helpers |
| `ungula::core::system::HealthMonitor` / `HealthSample` | `ungula/core/system/health_monitor.h` | Heap sampler |
| `ungula::core::system::ChipInfo` | `ungula/core/system/chip_info.h` | MCU identity struct |
| `ungula::core::util::string_t`, `string_view_t`, `vector_string_t`, `string_vector_t`, `vector_string_view_t` | `ungula/core/util/string_types.h` | std-aliases used across all libraries (`vector_string_t` and `string_vector_t` are the same type) |
| `ungula::core::control::PidConfig` | `ungula/core/control/pid.h` | PID gains and anti-windup limit |
| `ungula::core::control::Pid` | `ungula/core/control/pid.h` | Single-loop PID controller |

Time aliases at namespace scope (`ungula::core::time::tick_ms_t`,
`tick_us_t`, `duration_ms_t`, `duration_us_t`, `epoch_ms_t`) — all
`int64_t`. Names exist to make intent visible at call sites; types are
interchangeable.

`SystemControl` is non-instantiable (constructors deleted, all members
`static`).

---

## Public functions / methods

### `ungula::core::time` (selected)

All free functions in the `ungula::core::time` namespace. There is no
class to instantiate — call them directly, or under a short alias
(`namespace tc = ungula::core::time;`).

- **`tick_ms_t millis()`** / **`tick_us_t micros()`**
  Monotonic since boot. Both `int64_t` — never wrap in any device
  lifetime.
- **`void delay(duration_ms_t)`** / **`delayMs`** / **`delayUs`**
  Block the current task. ESP32 backend yields via FreeRTOS for
  `delayMs`; `delayUs` busy-waits.
- **`void delayUntilMs(tick_ms_t& ref, duration_ms_t period)`**
  Wait until `ref + period`, then advance `ref` by `period`. Drift-free.
  Identical signature in `delayUntilUs`.
- **`void yield()`** — cooperative scheduler handoff (not just `delayMs(0)`).
  On ESP32 it maps to `vTaskDelay(1)` (exactly one RTOS tick — the
  minimum blocking interval that guarantees IDLE can run and feed the
  watchdog). On host it maps to `std::this_thread::yield()`.
- **`epoch_ms_t now() / nowUtc() / nowLocal()`**
  Wall-clock if a provider is installed and valid; otherwise
  monotonic-since-boot. `nowLocal()` adds the configured offset.
- **`epoch_ms_t nowInTz(int32_t offsetSeconds)`**
  One-shot conversion without touching the stored offset.
- **`tick_us_t nowUs()`** — plain alias for `micros()`. The provider hook is
  deliberately not applied here.
- **`bool hasValidWallClock()`** — `true` when a provider is installed and
  reports `isValid()`, i.e. `now()` is real UTC epoch-ms rather than the
  monotonic boot fallback. Use it to decide between rendering a date and
  showing a placeholder, without naming the backend (NTP, RTC, GPS).
- **`void setTimeProvider(ITimeProvider*)`** /
  **`clearTimeProvider()`** — install/remove the wall-clock source. The
  pointer is stored, not copied — the provider must outlive the call.
- **`void setTimezone(tz::Timezone)`** /
  **`setTimezoneOffsetSeconds(int32_t)`** /
  **`int32_t timezoneOffsetSeconds()`**
- **`size_t formatUtc(char*, size_t)`** /
  **`formatLocal(char*, size_t)`** /
  **`formatNow(char*, size_t, const char* strftimeFmt)`**
  All return `0` when no valid provider is installed (formatting a
  monotonic tick as a date would print "1970-…" otherwise). The 5-arg
  `format(buf, size, fmt, epochSec, offset)` from `time_format.h`
  handles arbitrary stored timestamps.
- **`void setSyncTime(tick_ms_t remoteMs)`** /
  **`void setSyncTimeUs(tick_us_t remoteUs)`** / **`tick_ms_t syncNow()`** /
  **`tick_us_t syncNowUs()`** / **`int64_t syncOffset()`** /
  **`int64_t syncOffsetUs()`** / **`bool isSynced()`** / **`void clearSync()`**
  Network-coordinator clock alignment. `setSyncTime` stores a single
  offset; reads add it on the hot path. The ms and us offsets are
  independent — `setSyncTime` does not populate the us offset and vice
  versa, though either setter flips `isSynced()` to true.

### `ungula::core::time::ITimeProvider`

- **`int64_t nowMs() const`** — provider-reported current wall-clock in ms.
- **`bool isValid() const`** — when false, `time::now()` falls back to local monotonic clock.

### `ungula::core::time` formatting helpers (`time_format.h`)

- **`size_t format(char*, size_t, const char* fmt, time_t epochSec, int32_t offsetSec = 0)`**
  — pure formatter over `gmtime_r`; the offset is applied arithmetically and
  the system `TZ` is never consulted. Returns the character count, or `0` for
  a null buffer/format, `bufSize == 0`, a too-small buffer, or
  `epochSec <= 0` (treated as "no valid time"; the buffer is set to `""`).
- **`size_t formatIso8601(char*, size_t, time_t epochSec, int32_t offsetSec = 0)`**
  — convenience wrapper for `"%Y-%m-%d %H:%M:%S"`. Needs a 20-byte buffer.

### `ungula::core::preferences::IPreferences` / `Preferences`

Host-facing include: `ungula/core/preferences/preferences.h`.

- `IPreferences` lives at `ungula/core/preferences/i_preferences.h`.
- `Preferences` is a compile-time alias defined in
  `ungula/core/preferences/preferences.h`. It resolves to the platform's
  concrete implementation (`Esp32Preferences` on ESP-IDF). App code
  should always spell the type as `Preferences` so that switching
  platforms requires no source changes.
- `Esp32Preferences` lives at
  `ungula/core/preferences/platforms/esp32_preferences.h` and is
  compiled only on ESP32 macro branches (`ESP_PLATFORM`,
  `ARDUINO_ARCH_ESP32`, `ESP32`). Don't reference it by name from
  app code — go through the `Preferences` alias.

- **`bool begin(const char* ns)`** — open NVS namespace (≤ 15 chars).
  Must succeed before any get/put.
- **`void end()`** — close namespace; pair with each `begin`.
- **`bool putString / putBytes / putUInt8 / putUInt32`** — write.
- **`size_t getString(key, buf, bufSize)`** /
  **`getBytes(key, buf, bufSize)`** — return bytes read, `0` if missing.
- **`uint8_t / uint32_t getUInt8/32(key, defaultVal)`** — typed reads.
- **`bool remove(key)`**, **`bool clear()`**, **`bool hasKey(key)`**.

`Esp32Preferences` is the only concrete shipped implementation; it
compiles only on ESP32 macro branches and is reached through the
`Preferences` alias. STM32 branches are currently explicit
compile-time errors until a backend is added. Inject `IPreferences&`
into code that needs to be host-testable.

`Esp32Preferences::hasKey()` probes all key types used by this library
(`blob`, `str`, `u8`, `u32`) because ESP-IDF v5.1 has no
type-agnostic "exists" call.

### `ungula::core::preferences::programs::ProgramStore<ProgramT, MaxPrograms>`

Header path:
`ungula/core/preferences/tools/programs/program_store.h`.

`ProgramT` must be POD with `char name[N]` and `bool valid` fields.

- **`explicit ProgramStore(IPreferences&)`** — borrow-only reference.
- **`void init(const char* defaultName, ProgramT (*createDefault)(const char*))`**
  Load all slots, validate CRCs, auto-create slot 0 from
  `createDefault` if no valid program exists.
- **`const ProgramT& getProgram(int index) const`** — clamps to slot 0 on
  out-of-range input (does NOT return null).
- **`bool saveProgram(int index, const ProgramT&)`** — overwrites slot,
  forces `valid = true`, persists.
- **`bool deleteProgram(int index)`** — refuses to delete the last
  valid slot. Reassigns `activeIndex_` if the active slot is deleted.
- **`int getActiveIndex()`** / **`setActiveIndex(int)`** — active slot
  is rejected if it points to an invalid program.
- **`int getLastUsedIndex()`** / **`setLastUsedIndex(int)`**.
- **`int countValid()`**, **`static constexpr int maxPrograms()`**.

CRC32 is computed across the raw bytes of `ProgramT`. Any change to
the struct layout (added field, reordering, padding shift) invalidates
existing slots silently — bump a project version field inside
`ProgramT` if you want explicit migration.

Slot keys are `"p<index>"` written into a 4-byte buffer, so `MaxPrograms`
must stay **≤ 100**: index 100 truncates to `"p10"` and collides with slot 10.
Slot metadata (`activeIndex`, `lastUsedIndex`) is persisted as `uint8_t`, and
`255` is the "no last-used slot" sentinel — another reason to keep
`MaxPrograms` small. `saveProgram` returns `true` once the in-RAM slot is
updated; it does not report a failed storage write.

### `ungula::core::preferences::NvsConfigStore<ConfigT>`

Header: `ungula/core/preferences/nvs_config_store.h`.

Persist one POD config struct with a CRC32 checksum. On a CRC mismatch
the stored blob is replaced with the defaults (self-healing). Layout on
disk: `[ConfigT config][uint32_t crc]`.

`ConfigT` must be trivially copyable. The store borrows `IPreferences`
— the caller owns the namespace and key.

`ConfigLoadStatus` enum: `Loaded` (valid blob read), `Defaulted`
(nothing stored or wrong size), `Recovered` (CRC mismatch, defaults
rewritten).

- **`NvsConfigStore(IPreferences&, const char* ns, const char* key)`**
  — borrows the preferences reference and stores ns/key.
- **`ConfigT load(const ConfigT& defaults, ConfigLoadStatus* status = nullptr)`**
  — reads the blob; returns `defaults` on miss or CRC failure.
  Pass `status` to learn which happened. On `Recovered`, defaults are
  also written back.
- **`bool save(const ConfigT& config)`** — writes `config` + CRC.
  Returns `false` if the namespace could not be opened or the write
  failed.

```cpp
#include <ungula/core/preferences/nvs_config_store.h>
#include <type_traits>

namespace prefs_ns = ungula::core::preferences;

struct MotorConfig {
    float kp = 1.0f;
    float ki = 0.1f;
    float kd = 0.05f;
    float max_speed = 200.0f;
};

static_assert(std::is_trivially_copyable<MotorConfig>::value,
              "NvsConfigStore requires trivially copyable ConfigT");

prefs_ns::NvsConfigStore<MotorConfig> motorCfg(prefs, "motor", "config");

prefs_ns::ConfigLoadStatus status = prefs_ns::ConfigLoadStatus::Defaulted;
MotorConfig loaded = motorCfg.load({}, &status);   // status says where it came from
motorCfg.save({ 2.0f, 0.2f, 0.1f, 180.0f });
```

The store does **not** `static_assert` on `ConfigT` — add your own
`is_trivially_copyable` assert as above. A `ConfigT` with padding is stored
byte-for-byte including the padding, so keep the fields ordered largest-first
if you care about blob size.

### `ungula::core::preferences::counters::CounterStore`

Header: `ungula/core/preferences/tools/counters/counter_store.h`.

Named persistent `uint32_t` counters, one storage key each, inside a
dedicated namespace (default `"counters"`). Backend-agnostic — it only
talks to the injected `IPreferences`.

The class holds no state beyond the reference and the namespace, so it
can be built at the call site: `CounterStore(prefs).increment("runs");`

- **`CounterStore(IPreferences&, const char* ns = DEFAULT_NAMESPACE)`**
  — borrows the preferences reference.
- **`uint32_t read(const char* name) const`** — current value; `0` when
  the counter does not exist, the name is invalid, or the backend
  refuses to open. Never writes.
- **`bool write(const char* name, uint32_t value) const`** — sets the
  value, creating the counter on first use.
- **`uint32_t increment(const char* name, uint32_t step = 1) const`** —
  read + add + write. Returns the new stored value, or `0` on failure
  (a successful increment can never return `0`; values saturate at
  `MAX_VALUE` instead of wrapping).
- **`bool reset(const char* name) const`** — `write(name, 0)`. The key
  survives, so `exists()` stays true.
- **`bool exists(const char* name) const`** — distinguishes "never
  counted" from "counted zero times".
- **`static bool isValidName(const char* name)`** — 1..`MAX_NAME_LEN`
  (15) chars. Longer names are rejected, not truncated: two names
  sharing the first 15 chars would collide into one counter.

Constants: `DEFAULT_NAMESPACE` (`"counters"`), `MAX_NAME_LEN` (15),
`MAX_VALUE` (`UINT32_MAX`).

Not thread-safe — the injected `IPreferences` owns a single backend
handle. Each call is `begin()` → op → `end()`, so it never leaves the
namespace open.

```cpp
#include <ungula/core/preferences/tools/counters/counter_store.h>

namespace counters = ungula::core::preferences::counters;

counters::CounterStore machineCounters(prefs);
counters::CounterStore nodeCounters(prefs, "cnt_node");   // separate namespace

machineCounters.increment("boots");
const uint32_t runs = machineCounters.increment("runs");
```

Cost model: one storage entry per write. Fine for per-run / per-boot events;
never increment inside a control loop — accumulate in RAM and persist the
total once when the run ends.

### `ungula::core::util::Queue<T, Capacity>`

- **`bool push(const T&)`** / **`bool push(T&&)`** — `false` when full.
- **`bool pop(T& out)`** — `false` when empty; moves into `out`.
- **`bool peek(T& out) const`** — copy of the front element.
- **`size_t count()`**, **`bool isFull()`**, **`bool isEmpty()`**,
  **`constexpr size_t capacity()`**, **`void clear()`**.

`noexcept` throughout. Storage is a `T data_[Capacity]` member — no
heap. Not thread-safe; wrap with a mutex or use one queue per
producer/consumer pair.

### `ungula::core::util::str` (selected, namespace `ungula::core::util::str`)

`trim(string_t&)`, `as_trim`, `to_lower`, `as_lower`, `to_upper`,
`as_upper`, `startsWith`, `replaceAll`, `escapeString`, `countChar`,
`countTokensByChar`, `tokenizeByDelimiter`, `cleanDelimitedValues`,
`string_indexOf`, `string_substring`, `string_equals`,
`num_to_string<T>`, `num_to_stringf<T>`, `skipWhitespace*`,
`trimWhitespace`. All inline.

### `ungula::core::util` math/temp helpers (in `ungula/core/util/types.h`)

- `math::clamp` — return clamped value, no side effects.
- `math::clamp_v` — modify the first argument by
  reference.
- `math::clamp01 / lerp`.
- `temp::packCelsius(float celsius) -> int16_t` — multiplies by 10 and
  rounds for wire transmission (0.1 °C resolution). Pairs with
  `unpackCelsius`.
- `temp::unpackCelsius(int16_t packed) -> float` — divides by 10 to
  restore Celsius from a packed wire value.
- `temp::celsiusToFahrenheit / fahrenheitToCelsius` (double and
  float variants), `isValidTemperature(C)` returns false for non-finite
  or values outside `(-200°C, 1800°C)`.
- `enums::toUint8<E>()`, `enums::fromUint8<E>(uint8_t)` — generic enum
  conversion.

### CRC32

- **`uint32_t ungula::core::util::crc32(const uint8_t* data, size_t len)`**
- **`uint32_t ungula::core::util::crc32_byte(uint32_t crc, uint8_t byte)`** —
  step function for streaming.

Polynomial `0xEDB88320` (zlib/Ethernet). Initial `0xFFFFFFFF`,
final XOR `0xFFFFFFFF`.

### `ungula::core::system::SystemControl`

- **`static void reboot()`** — immediate hard reset.
- **`static void rebootAfterMs(uint32_t)`** — sleeps then reboots.

Implementation status: ESP32-only (`esp_restart`). Non-ESP builds that
compile this translation unit fail by design (`#error "Unsupported platform"`).

### `ungula::core::system::HealthMonitor`

- **`bool sample(uint32_t intervalMs, HealthSample& out)`** — fills
  `out` and returns `true` only when at least `intervalMs` have passed
  since the previous sample. The first call always returns `true` with
  `delta == 0`. `out` is left untouched when it returns `false`.
- **`void reset()`** — clear baseline (use after a planned big
  alloc/free); the next `sample()` fires immediately with `delta == 0`.

`HealthSample` fields: `uint32_t free_heap`, `uint32_t min_free_heap`,
`int32_t delta` (current minus previous, `0` on the first sample),
`uint32_t uptime_ms`.

Note `uptime_ms` is `uint32_t`, not one of the `int64_t` time aliases — it
wraps after ~49.7 days of uptime. The interval gate itself is unaffected
(unsigned subtraction wraps correctly), but do not use the field as a
long-running absolute timestamp.

### `ungula::core::system::queryChipInfo()`

Returns a `ChipInfo` by value, populated by the platform backend. Strings are
fixed-size in-struct buffers, so the value can be stored or copied freely.

`ChipInfo` fields: `char model[24]`, `char sdkVersion[24]`,
`char features[80]` (human-readable, e.g. `"WiFi, BT, BLE, PSRAM"`),
`uint8_t cores`, `uint8_t revision`, `bool hasWifi`, `bool hasBluetooth`,
`bool hasBle`, `bool hasPsram`. Buffer sizes are exposed as
`CHIP_MODEL_MAX_LEN`, `SDK_VERSION_MAX_LEN`, `CHIP_FEATURES_MAX_LEN`.

Implementation status: ESP32-only. `chip_info.cpp` includes ESP-IDF headers
unconditionally, so do not add it to a host build.

### `ungula::core::time::tz`

Header: `ungula/core/time/timezones.h`.

- **`enum class Timezone : uint8_t`** — 40 entries (UTC, GMT, CET,
  EST, JST, …). Disambiguated suffixes for clashes (`CST_NA`,
  `CST_CN`, `IST_IN`, `AST_ATL`, `BST_UK`).
- **`constexpr int32_t offsetSeconds(Timezone)`** — fixed offset
  from UTC. No DST awareness. Enum values are stable across versions
  (entries can be added at the end only).
- **`constexpr const char* abbreviation(Timezone)`** — short code
  (`"JST"`, `"CST"`). Region-suffixed enums collapse to the plain code,
  so `CST_NA` and `CST_CN` both return `"CST"`.
- **`constexpr const char* name(Timezone)`** — friendly region
  (`"US Pacific"`, `"Japan"`).
- **`struct Entry { Timezone tz; int32_t offsetSeconds; const char* abbreviation; const char* name; }`**
  and **`inline constexpr Entry TIMEZONES[]`** — the single source of
  truth behind the three lookups. Iterate it to build a picker UI.

All three lookups are a linear scan that returns the first match, and fall
back to UTC / `"UTC"` for an enum value with no row.

### `ungula::core::control::Pid`

Header: `ungula/core/control/pid.h`.

`PidConfig` struct: `kp` (proportional gain), `ki` (integral gain), `kd` (derivative gain), `i_max` (anti-windup clamp on `|integral|`).

- **`explicit Pid(const PidConfig&)`** — copies the config into the object;
  the caller's `PidConfig` does not need to outlive it.
- **`float update(float error, float dt_s)`**
  Runs one iteration. `dt_s` must be `> 0` — **the value is not validated**,
  and `dt_s == 0` divides by zero in the derivative term and returns
  `inf`/`NaN`. Returns `kp·e + ki·∫e + kd·de/dt` (unclamped). First call
  produces `deriv = 0`.
- **`void setConfig(const PidConfig&)`** — replace the gains at runtime
  (operator tuning). The integral accumulator survives the change, so a
  `reset()` right after is the usual choice.
- **`const PidConfig& config() const`** — current gains; reference into the
  object, valid until the next `setConfig()`.
- **`void reset()`** — clears the integral, the previous error, and the
  primed flag. Call after setpoint jumps, loop disable, or hard-stop events.
- **`float integral() const`** — current integral accumulator value (for logging or tuning).

Anti-windup: `integral_` is clamped to `[-i_max, +i_max]` before the derivative is computed, so a saturated output does not poison future ticks.

Output is intentionally unclamped — callers apply `[min, max]` after adding base speed/bias.

---

## Lifecycle

- **Time API (`ungula::core::time`)** — no init required for
  `millis`/`micros`/`delay*`. For `now()` to return wall-clock, install
  a provider with `setTimeProvider` and ensure `isValid()` returns
  `true` before the first call. `setTimezone` is a one-shot
  configuration.
- **Preferences** — every read/write must be sandwiched in
  `begin(ns)` … `end()`. On ESP32 (`Esp32Preferences`), NVS init
  (`nvs_flash_init`) must happen before the first `begin`. Forgetting
  `end()` leaks an NVS handle.
- **ProgramStore** — call `init()` exactly once in `setup()`. Must run
  after the underlying `IPreferences` is usable. `saveProgram` and
  `deleteProgram` persist immediately; metadata (`activeIndex`,
  `lastUsedIndex`) is also persisted on every change.
- **NvsConfigStore** — no `init()` required. Construct with the
  `IPreferences` reference, namespace, and key; call `load()` or
  `save()` as needed. The caller opens/closes the namespace internally.
- **CounterStore** — no `init()` required, and counters are created on
  first `write`/`increment`/`reset`. Stateless besides the reference and
  namespace, so it can live as a member or be built at the call site.
- **HealthMonitor** — single instance per project, sampled from
  `loop()`. No init required.
- **Queue** — value-initialized; no init required.
- **Pid** — value-initialized; no init required. Call `reset()` after
  setpoint changes or loop disable.

No object in this library uses `new`/`delete` after construction.

---

## Error handling

- **No exceptions used in public APIs.** Failures surface as `bool`
  return values, `0` byte-count returns, or out-of-band sentinel values.
- `IPreferences::getString` / `getBytes` — return `0` when the key
  is missing or the buffer is too small.
- `IPreferences::getUInt*` — return `defaultVal` when the key is
  missing.
- `ProgramStore::getProgram(index)` — out-of-range index falls back to
  slot 0; check `getProgram(idx).valid` to distinguish "real slot 0"
  from "fallback slot 0".
- `Queue::push/pop/peek` — `false` on full/empty; never block.
- `tc::format*` — return `0` when no valid time provider is
  installed (caller must check before treating the buffer as printable).
- `tz::offsetSeconds` — undefined enum values return `0` (UTC).
- `Pid::update(error, dt_s)` — `dt_s <= 0` produces undefined derivative
  (caller is responsible for ensuring `dt_s > 0`).
- `NvsConfigStore::load` — always returns a valid config (either stored or
  defaults). Use the `ConfigLoadStatus*` parameter to distinguish sources.
  On `Recovered`, defaults are written back automatically.
- `NvsConfigStore::save` — returns `false` if the namespace could not be
  opened or the write failed.
- `CounterStore::read` — returns `0` for a missing counter, an invalid
  name, or a backend that refused to open. Use `exists()` when the
  difference matters.
- `CounterStore::increment` — returns `0` on failure only; a successful
  increment always returns `>= 1` because values saturate at
  `MAX_VALUE` instead of wrapping.
- `CounterStore::write` / `reset` — `false` on an invalid name or a
  backend failure.

---

## Threading / timing / hardware notes

- **tc::millis / micros**: ESP32 backend uses
  `esp_timer_get_time()` — ISR-safe and lock-free. Host backend uses
  `std::chrono::steady_clock`.
- **tc::delayMs**: ESP32 yields via FreeRTOS
  (`vTaskDelay`-equivalent). Host backend uses
  `std::this_thread::sleep_for`. Do NOT call from inside an ISR.
- **tc::delayUs**: busy-wait on ESP32 — does not yield.
  Acceptable up to a few hundred microseconds.
- **tc::yield**: on ESP32 this is one RTOS tick (`vTaskDelay(1)`),
  intentionally chosen so IDLE can run and watchdog feeding is not starved.
  On host it maps to `std::this_thread::yield()`.
- **Module state in `ungula::core::time::detail::`** (`provider_`,
  `sync_`, `timezoneOffsetSeconds_`): not protected by a mutex.
  Configure once during `setup()` from a single task; readers can be
  concurrent.
- **Queue**: not thread-safe. One producer + one consumer is fine
  only if you accept the standard SPSC caveats; for ISR↔task use a
  FreeRTOS queue or wrap with `portENTER_CRITICAL`.
- **NVS access** is mutex-protected by ESP-IDF, but `Preferences`
  is single-instance-per-namespace by design — do not share one
  `Preferences` object across tasks.
- **CounterStore**: inherits the `Preferences` rule above — all counter
  calls from the same task that owns the injected instance. Every call
  costs one storage write when it writes; count per-event, not per-loop.
- **CRC32** functions are pure and reentrant.
- **HealthMonitor::sample** reads `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)`
  / `heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)` on ESP32 — cheap,
  but not zero-cost; the interval gate exists for that reason. On host both
  return `0`, so every sample reports `free_heap == 0, delta == 0`.

---

## Internals not part of the public API

- `ungula/core/time/platforms/time_control_esp32.h`,
  `ungula/core/time/platforms/time_control_host.h` — picked automatically by
  `time.h`. Never `#include` directly.
- `ungula/core/preferences/platforms/*` in generic domain code — inject
  `IPreferences` instead. Use platform headers only in composition-root code.
- `ungula::core::time::detail::SyncState` and the inline-static storage
  members (`provider_`, `sync_`, `timezoneOffsetSeconds_`) —
  implementation detail of the namespace; do not reach into them.
- `ProgramStore::ProgramBlob`, `computeCrc`, `loadAllFromNvs`,
  `saveProgramToNvs`, `loadMetaFromNvs`, `saveMetaToNvs`,
  `programKey`, `NVS_NS = "programs"` — internal layout. Do not
  reach into NVS for these keys directly.
- `ungula/core/preferences/platforms/esp32_preferences.cpp` — the `nvs_flash` glue.
  Use the `IPreferences` interface; never include this file from app code.
---

## LLM usage rules

The per-subsystem Do/Don't checklists near the top are the detailed version.
The ones that are easy to get wrong and are not repeated there:

- Treat `IPreferences` as the only persistence interface. Depend on the
  abstract type in domain code, not on the `Preferences` alias or on
  `Esp32Preferences`. One namespace per object.
- Prefer `ProgramStore` / `NvsConfigStore` over hand-rolled NVS
  serialization, and never touch the `programs` namespace by hand.
- Use `Queue<T, N>` instead of `std::deque` or a dynamic ring buffer.
- Use `string_t` / `string_view_t` and the `str::` helpers instead of Arduino
  `String`. `string_indexOf` / `string_substring` / `string_equals` are a
  porting aid — prefer the `std::string` members in new code.
- Don't add logging into this library; surface state via return values or
  callbacks (project rule).
