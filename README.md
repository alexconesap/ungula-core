# UngulaCore

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — core utilities (time, preferences, CRC, queues, strings).

> **LLM usage note:** if this library is consumed from a coding AI workflow, explicitly point the agent to `API.md` first. `API.md` is the LLM-facing contract (public API + examples + constraints) and avoids wasting time/tokens scanning source files and this human-oriented README.

> **Warning - Active Development:** This library is under active architecture work to support multiple projects in parallel. Its structure is not finalized yet and may change without notice while this work is in progress. Updates are currently frequent (often daily). Target for structural freeze and stable `v1.0.0`: **June 2026**.

Generic utility library for embedded projects, fully portable. It handles persistent storage, time control/delays, system control, and various utility functions. It never logs — the host project decides what to do with the values it returns.

When I recently started working on an existing Arduino-based C++ project that used only the ESP32 hardware, I quickly realized how much work it takes to migrate from Arduino libraries to the ESP32 SDK. To do this gradually, I began implementing this library, which allows me to easily port projects that still rely on certain Arduino libraries (for example, to control specific motor drivers or sensors) while accessing ESP32 libraries natively to improve overall performance.

Ultimately, my goal is to port 100% of the code from one hardware platform for example STM to another one such as ESP with minimal impact, keeping the system fully abstracted and introducing minimal overhead. A key objective is also to eliminate Arduino libraries that are not optimized for any specific platform.

## Table of Contents

- [C++ Compatibility](#c-compatibility)
- [Quick Start](#quick-start)
- [Time Control (`ungula/core/time/`)](#time-control-ungulacoretime)
  - [Calling style](#calling-style)
  - [Periodic Loop Without Drift](#periodic-loop-without-drift)
  - [Simple Delays and cross-platform time functions](#simple-delays-and-cross-platform-time-functions)
  - [Useful helpers](#useful-helpers)
  - [Wall-clock vs monotonic — what each call returns](#wall-clock-vs-monotonic--what-each-call-returns)
  - [Pluggable time source (`ungula/core/time/i_time_provider.h`)](#pluggable-time-source-ungulacoretimei_time_providerh)
  - [Coordinator-synced clock](#coordinator-synced-clock)
  - [Formatting (`ungula/core/time/time_format.h`)](#formatting-ungulacoretimetime_formath)
- [System Control (`ungula/core/system/`)](#system-control-ungulacoresystem)
  - [Chip Info (`ungula/core/system/chip_info.h`)](#chip-info-ungulacoresystemchip_infoh)
  - [Health Monitor (`ungula/core/system/health_monitor.h`)](#health-monitor-ungulacoresystemhealth_monitorh)
- [Control (`ungula/core/control/`)](#control-ungulacorecontrol)
  - [PID Controller (`ungula/core/control/pid.h`)](#pid-controller-ungulacorecontrolpidh)
- [Utilities](#utilities)
  - [CRC32 (`ungula/core/util/crc32.h`)](#crc32-ungulacoreutilcrc32h)
  - [Queue (`ungula/core/util/queue.h`)](#queue-ungulacoreutilqueueh)
  - [String Utilities (`ungula/core/util/string_utils.h`)](#string-utilities-ungulacoreutilstring_utilsh)
  - [String types (`ungula/core/util/string_types.h`)](#string-types-ungulacoreutilstring_typesh)
  - [Temperature & Math (`ungula/core/util/types.h`)](#temperature--math-ungulacoreutiltypesh)
- [Preferences (`ungula/core/preferences/`)](#preferences-ungulacorepreferences)
  - [Persistent Key-Value Storage](#persistent-key-value-storage)
  - [Program Store (Recipe Manager)](#program-store-recipe-manager)
  - [Config Store (Single Config)](#config-store-single-config)
  - [Counters (Named Lifetime Tallies)](#counters-named-lifetime-tallies)
- [Testing](#testing)
- [Acknowledgements](#acknowledgements)
- [License](#license)
- [Arduino CLI symlink note (rarely relevant)](#arduino-cli-symlink-note-rarely-relevant)

## C++ Compatibility

- **Own source minimum**: `C++17`.
- **Effective minimum for consumers**: `C++17`.
- **Dependency impact**: None (no declared internal dependencies).

## Quick Start

In your Arduino `.ino` file:

```cpp
#include <ungula/core.h>
```

Then include whatever component you need in your code:

```cpp
#include "ungula/core/preferences/preferences.h"
#include "ungula/core/util/string_utils.h"
#include "ungula/core/util/queue.h"
#include "ungula/core/util/crc32.h"
#include "ungula/core/time/time.h"
#include "ungula/core/system/system_reboot.h"
...
```

## Time Control (`ungula/core/time/`)

Portable time abstraction. Free functions in `ungula::core::time` — no class to instantiate. `time.h` is the only header your code ever includes; it dispatches to a platform-specific backend at build time:

```text
ungula/core/time/
  time.h                               # public API (unchanged)
  i_time_provider.h                    # pluggable clock source
  platforms/
    time_control_esp32.h               # ESP-IDF: FreeRTOS + esp_timer
    time_control_host.h                # std::chrono + std::thread (tests, STM32 fallback)
```

Selection happens at the bottom of `time.h` via a single `#if defined(ESP_PLATFORM)`. Adding a new platform (STM32, etc.) means dropping one more file into `platforms/` and adding one `#elif` branch — no cross-platform `#ifdef` clutter inside any implementation file.

### Calling style

The API is a flat set of free functions in `ungula::core::time`. Use them directly:

```cpp
#include <ungula/core.h>

ungula::core::time::delay(2000);
ungula::core::time::delayUs(500);
auto t = ungula::core::time::millis();
```

…or under a short namespace alias if you call them often:

```cpp
#include <ungula/core.h>

namespace tc = ungula::core::time;
tc::delay(2000);
auto t = tc::millis();
```

There is no class involved — every call is a free function on the namespace.

### Periodic Loop Without Drift

This is the main use case. Instead of doing `delay(10)` at the end of a loop (which accumulates drift from the work time), use `delayUntilMs`. It advances the reference by the period, not by "now + period", so you get consistent timing:

```cpp
#include "ungula/core/time/time.h"

namespace tc = ungula::core::time;

auto ref = tc::millis();
while (running) {
    readSensors();  // takes 3ms
    sendStatus();   // takes 4ms

    // delay(50) would wait 50ms on top of the 7ms of work = 57ms total.
    // delayUntilMs waits only 43ms — the time remaining to reach 50ms.
    tc::delayUntilMs(ref, 50);  // 50ms period, drift-free
}
```

Same thing in microseconds (best-effort, busy-wait):

```cpp
#include <ungula/core.h>

auto ref = tc::micros();
while (stepping) {
    toggleStepPin();
    tc::delayUntilUs(ref, 200);  // 200us period
}
```

### Simple Delays and cross-platform time functions

```cpp
#include <ungula/core.h>

tc::delayMs(100);           // block for 100ms
tc::delayUs(500);           // block for 500us (busy-wait)
auto now = tc::millis();    // monotonic ms tick
auto us  = tc::micros();    // monotonic us tick
```

### Useful helpers

```cpp
#include <ungula/core.h>

// Stop using
// Same time, for the developer reading your code (or yourself some day in the future), can lead to think
// why 0?
tc::delayMs(0);
// BTW It will not really work! The min ticks to wait depend on the platform and to wait 0ms means no wait at all

// You can express better your intent and avoid errors by using
tc::yield();
```

When using a `tc::delayMs(0)` you will typically see this error in a ESP32:

```text
E (xxxxx) task_wdt: Task watchdog got triggered.
E (xxxxx) task_wdt: The following tasks did not reset the watchdog in time:
E (xxxxx) task_wdt:  - loopTask (CPU 1)
E (xxxxx) task_wdt: Tasks currently running:
E (xxxxx) task_wdt: CPU 0: IDLE0
E (xxxxx) task_wdt: CPU 1: loopTask
E (xxxxx) task_wdt: Aborting.
```

### Wall-clock vs monotonic — what each call returns

All time values are signed `int64_t`. See "Why `int64_t`" below for the rationale.

| Call | Type | Source | Use case |
| --- | --- | --- | --- |
| `millis()`, `micros()` | `tick_ms_t` / `tick_us_t` | local monotonic, never wraps in practice | delays, timeouts, interval math |
| `now()` / `nowUtc()` | `epoch_ms_t` | `ITimeProvider` if installed and valid, else `millis()` | wall-clock UTC for logs, scheduling |
| `nowLocal()` | `epoch_ms_t` | `now() + setTimezoneOffsetSeconds()` | wall-clock in the configured TZ |
| `nowInTz(offset_s)` | `epoch_ms_t` | `now() + offset_s * 1000` | one-off arbitrary TZ |
| `nowUs()` | `tick_us_t` | plain alias for `micros()` — never routed through the provider | high-resolution intervals |
| `hasValidWallClock()` | `bool` | provider installed **and** `isValid()` | decide between rendering a date and a placeholder |

`hasValidWallClock()` is the honest test for "is `now()` a real date". Without it, `now()` silently returns a monotonic boot tick that looks like an epoch in early 1970:

```cpp
if (tc::hasValidWallClock()) {
    char ts[24];
    tc::formatLocal(ts, sizeof(ts));
    showClock(ts);
} else {
    showClock("--:--:--");
}
```

UTC is the default. `setTimezoneOffsetSeconds(int32_t)` (or the named-zone overload `setTimezone(tz::Timezone)`) configures `nowLocal()`; `now()` / `nowUtc()` ignore it. No DST awareness — entries like `PST_NA` and `PDT_NA` are separate zones and the application picks which one is in effect.

#### Why `int64_t`

The aliases (`tick_ms_t`, `tick_us_t`, `duration_ms_t`, `duration_us_t`, `epoch_ms_t`) are all `int64_t`. Three reasons:

1. **No wrap.** `uint32_t` ms wraps every 49 days; `uint32_t` µs every 71 minutes. Devices that run continuously hit both. `int64_t` ms gives ~292 million years of headroom — effectively never.
2. **No silent truncation.** ESP-IDF's `esp_timer_get_time()` already returns `int64_t`. The previous `uint32_t` `micros()` was *wrong* past 71 minutes — values were truncated to the low 32 bits. Now the call is straight pass-through.
3. **Signed math just works for deadlines.**

```cpp
#include <ungula/core.h>

   const auto remaining = deadline - tc::millis();
   if (remaining <= 0) handleOverdue();   // intuitive
```

With unsigned, an overdue value becomes a huge positive number and the code "waits" for ~49 days. Signed types eliminate that whole class of bug.

The three aliases exist to make intent visible — same width, different meanings:

- `tick_ms_t` — a moment captured from `millis()`.
- `duration_ms_t` — an interval (delay length, remaining time).
- `epoch_ms_t` — a wall-clock instant since the Unix epoch.

#### Named timezones (`ungula/core/time/timezones.h`)

Project code should not have to remember that Tokyo is `9 * 3600`. Pick from the `Timezone` enum and pass it to `setTimezone()`. UTC is the default — call this only if the device should report wall time in some other zone.

```cpp
#include <ungula/core/time/time.h>
#include <ungula/core/time/timezones.h>

namespace tz = ungula::core::time::tz;

tc::setTimezone(tz::Timezone::UTC);     // back to UTC (the default)
tc::setTimezone(tz::Timezone::JST);     // Tokyo  (+9:00)
tc::setTimezone(tz::Timezone::IST_IN);  // India  (+5:30)
```

##### Example — device deployed in Los Angeles

Pacific time observes DST: standard `PST_NA` (-8:00) in winter, daylight `PDT_NA` (-7:00) in summer. The library does not switch automatically — the application picks which one is in effect. Wire the choice to whatever signal makes sense for the project (a button, a config value, an NTP-derived `tm_isdst` flag, a date check):

```cpp
#include <ungula/core/time/time.h>
#include <ungula/core/time/timezones.h>

namespace tz = ungula::core::time::tz;

void applyLosAngelesZone(bool daylightSaving) {
    tc::setTimezone(daylightSaving ? tz::Timezone::PDT_NA
                                            : tz::Timezone::PST_NA);
}

void setup() {
    // ...NTP / boot...
    applyLosAngelesZone(/*daylightSaving=*/false);  // winter
}

void onEnterDaylightSaving() {
    applyLosAngelesZone(true);
}
```

After the call, `tc::nowLocal()` returns LA wall-clock ms; `tc::nowUtc()` is unaffected.

##### Example — device deployed in Barcelona / Spain

Spain follows Central European Time. Winter is `CET` (+1:00), summer is `CEST` (+2:00). Same pattern:

```cpp
#include <ungula/core.h>

namespace tz = ungula::core::time::tz;

void applyBarcelonaZone(bool daylightSaving) {
    tc::setTimezone(daylightSaving ? tz::Timezone::CEST
                                            : tz::Timezone::CET);
}

void setup() {
    applyBarcelonaZone(/*daylightSaving=*/true);  // summer
}
```

##### Multi-zone read (without changing the configured zone)

If the device is configured for one zone but a single read needs another (e.g. a UI tooltip showing "in Barcelona, that's…" while the host project runs in LA), use `nowInTz()` — it does not touch the stored offset:

```cpp
#include <ungula/core.h>

tc::setTimezone(tz::Timezone::PST_NA);  // device is in LA

const tc::epoch_ms_t bcnNow = tc::nowInTz(
        tz::offsetSeconds(tz::Timezone::CET));    // one-off Barcelona view
const tc::epoch_ms_t laNow  = tc::nowLocal(); // still LA — unchanged
```

##### Inventory

The full mapping lives in `ungula/core/time/timezones.h` as a `constexpr` table — about 40 commonly used abbreviations including UTC/GMT/WET, the European set (CET/CEST/EET/EEST/MSK/BST_UK), the Asia/Pacific set (IST_IN/CST_CN/SGT/JST/KST/AEST/AEDT/ACST/NZST/NZDT), and the Americas (EST/EDT/CST_NA/CDT_NA/MST_NA/MDT_NA/PST_NA/PDT_NA/HST/AKST/AKDT/AST_ATL/BRT/ART). DST-observing zones appear as separate entries (e.g. `PST_NA` and `PDT_NA`) — the application chooses which one is currently active.

`tz::offsetSeconds(zone)`, `tz::abbreviation(zone)` (short code, e.g. `"JST"`) and `tz::name(zone)` (friendly region, e.g. `"US Pacific"`) are exposed for callers that need the values directly without going through the time API. The backing `tz::TIMEZONES[]` table of `tz::Entry` is `constexpr` and public too — iterate it to build a zone picker.

### Pluggable time source (`ungula/core/time/i_time_provider.h`)

`tc::millis()` is always the local monotonic clock. `tc::now()` can be routed through a custom source by installing an `ITimeProvider`. Typical uses: an NTP-synced wall-clock, an external RTC chip, a mock clock in tests.

```cpp
#include "ungula/core/time/i_time_provider.h"
#include "ungula/core/time/time.h"

using ungula::core::time::ITimeProvider;
namespace tc = ungula::core::time;

/// Real-world example: a provider fed by the NTP sink elsewhere in the
/// firmware. Reports a current epoch time when synchronised, and falls
/// back to "invalid" until the first successful sync — at which point
/// tc::now() returns the local monotonic clock on its own.
class NtpTimeProvider final : public ITimeProvider {
    public:
        int64_t nowMs() const override {
            return current_epoch_ms_;
        }
        bool isValid() const override {
            return synchronized_;
        }

        /// Called by the NTP sink whenever a fresh sample lands.
        void onNtpSample(int64_t epochMs) {
            current_epoch_ms_ = epochMs;
            synchronized_ = true;
        }

        /// Called when the link drops or the sample goes stale.
        void invalidate() {
            synchronized_ = false;
        }

    private:
        int64_t current_epoch_ms_ = 0;
        bool synchronized_ = false;
};

NtpTimeProvider g_ntpProvider;

void setup() {
    tc::setTimeProvider(&g_ntpProvider);
    // now() returns local millis() until the first NTP sample lands.
}
```

Contract:

- The provider must outlive the `setTimeProvider()` call. the API stores the pointer, it does not copy.
- `isValid()` is checked on **every** `now()` call — not cached — so a provider can toggle its validity at runtime without re-registering.
- When `isValid()` returns false, `tc::now()` falls back to the local monotonic clock. No exception, no "frozen" value.
- `clearTimeProvider()` removes the provider and restores local-clock behaviour.
- `micros()` and `nowUs()` are **not** routed through the provider — microsecond-grade external sources are rare enough to not pay for the indirection.

`ITimeProvider` is independent from the `setSyncTime()` / `syncNow()` pair. The sync clock stores a fixed offset to a coordinator's millisecond timestamp; the provider replaces the clock source entirely. Pick one, not both, per deployment.

### Coordinator-synced clock

For a mesh where one node is the timing reference (ESP-NOW coordinator, for example), the sync clock aligns the local monotonic tick to the coordinator's without pretending to be a wall clock. One subtraction on sync, one addition per read:

```cpp
#include <ungula/core/time/time.h>

namespace tc = ungula::core::time;

void onCoordinatorBeacon(tc::tick_ms_t coordinatorMs) {
    tc::setSyncTime(coordinatorMs);          // stores coordinatorMs - millis()
}

void scheduleStep() {
    const tc::tick_ms_t common = tc::syncNow();   // millis() + offset
    // ...all nodes agree on 'common' to within the beacon jitter
}
```

| Call | Behavior |
| --- | --- |
| `setSyncTime(remoteMs)` / `setSyncTimeUs(remoteUs)` | Store the offset against `millis()` / `micros()`. Marks the clock synced. |
| `syncNow()` / `syncNowUs()` | Local tick plus the stored offset. |
| `syncOffset()` / `syncOffsetUs()` | The stored offsets, in ms and us. |
| `isSynced()` | True once either setter has run. |
| `clearSync()` | Zero both offsets and drop the flag. |

The ms and us offsets are independent: `setSyncTime()` leaves `syncOffsetUs()` at 0 and vice versa, even though either one flips `isSynced()` to true. Set whichever resolution you actually read.

### Formatting (`ungula/core/time/time_format.h`)

Formatting belongs to the time API, not to the time *source*. NTP, an RTC chip, or a fake clock all hand back a `time_t` epoch — turning that into "YYYY-MM-DD HH:MM:SS" is the same operation regardless. The pure helper lives in `ungula/core/time/time_format.h` and `ungula::core::time` exposes thin wrappers that pass its current state to it.

```cpp
#include <ungula/core/time/time.h>
#include <ungula/core/time/timezones.h>

namespace tz = ungula::core::time::tz;

tc::setTimezone(tz::Timezone::CET);  // device deployed in Barcelona

char ts[24];

// Current wall-clock time formatted as "YYYY-MM-DD HH:MM:SS":
tc::formatUtc(ts, sizeof(ts));    // "2026-04-23 14:32:11"
tc::formatLocal(ts, sizeof(ts));  // "2026-04-23 15:32:11" (CET)

// Custom strftime spec, in the configured zone:
tc::formatNow(ts, sizeof(ts), "%H:%M");  // "15:32"
```

Contract:

- All three return `0` when no `ITimeProvider` is installed and valid. The buffer is left untouched in that case (no misleading "1970-…" output sneaking into logs).
- `formatLocal()` and `format()` apply the offset configured via `setTimezone()` / `setTimezoneOffsetSeconds()`. `formatUtc()` ignores it.
- For arbitrary epoch values (formatting a stored timestamp from somewhere else, not "right now"), call `time_format::format()` / `formatIso8601()` directly — same helpers, but you supply the epoch yourself.

```cpp
#include <ungula/core/time/time_format.h>

char ts[24];
ungula::core::time::formatIso8601(ts, sizeof(ts), saved_epoch_seconds, /*offset=*/0);
```

## System Control (`ungula/core/system/`)

```cpp
#include <ungula/core.h>

using ungula::core::system::SystemControl;

SystemControl::reboot();
SystemControl::rebootAfterMs(500);  // wait 500ms then reboot
```

### Chip Info (`ungula/core/system/chip_info.h`)

Query the MCU at boot to log hardware details. The header is portable (plain struct, no SDK types). The implementation calls ESP-IDF internally.

```cpp
#include <ungula/core/system/chip_info.h>
#include <emblogx/logger.h>

void setup() {
    ungula::core::system::ChipInfo chip = ungula::core::system::queryChipInfo();
    log_info("MCU: %s rev%d, %d cores, IDF %s",
             chip.model, chip.revision, chip.cores, chip.sdkVersion);
    log_info("Features: %s", chip.features);

    if (chip.hasPsram) {
        log_info("PSRAM available");
    }
}
```

Fields: `model`, `sdkVersion`, `features` (human-readable string), `cores`, `revision`, `hasWifi`, `hasBluetooth`, `hasBle`, `hasPsram`. All strings are fixed-size in-struct buffers (`CHIP_MODEL_MAX_LEN`, `SDK_VERSION_MAX_LEN`, `CHIP_FEATURES_MAX_LEN`), so the struct can be copied around freely.

### Health Monitor (`ungula/core/system/health_monitor.h`)

Samples the free-heap counter on a gate so you can watch the trend. Firmware that allocates nothing after boot should show `free_heap` oscillating around a fixed value; a monotonic downward drift is a leak, and it is usually the only early signal you get.

The monitor collects numbers and nothing else — it does not log, that is the caller's job.

```cpp
#include <ungula/core/system/health_monitor.h>

static ungula::core::system::HealthMonitor health;

void loop() {
    ungula::core::system::HealthSample s;
    if (health.sample(60000U, s)) {   // at most once a minute
        log_info("heap free=%u min=%u delta=%ld up=%u",
                 s.free_heap, s.min_free_heap, (long)s.delta, s.uptime_ms);
    }
}
```

| Call | Behavior |
| --- | --- |
| `sample(intervalMs, out)` | Fills `out` and returns `true` only when `intervalMs` have passed. The first call always fires, with `delta == 0`. `out` is untouched when it returns `false`. |
| `reset()` | Drop the baseline, so the next `delta` is measured against a fresh sample. Use after a deliberate large alloc/free. |

`HealthSample` fields: `free_heap`, `min_free_heap` (lowest ever seen since boot), `delta` (signed, current minus previous), `uptime_ms`.

On host builds the heap counters are stubbed to `0`, so every sample reports zeros. On ESP32 they come from `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)` and `heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)`.

Note that `uptime_ms` is a `uint32_t`, unlike the `int64_t` time aliases — it wraps after ~49.7 days. The interval gate is not affected by the wrap, but do not treat the field as a long-running absolute timestamp.

## Control (`ungula/core/control/`)

### PID Controller (`ungula/core/control/pid.h`)

Generic single-loop PID controller. Output is unclamped — callers apply their own `[min, max]` clamp after adding their base speed or bias. The integral has its own anti-windup clamp (`i_max`) so a saturated output doesn't poison future ticks.

```cpp
#include <ungula/core/control/pid.h>

ungula::core::control::PidConfig cfg = { .kp = 1.0f, .ki = 0.1f, .kd = 0.05f, .i_max = 50.0f };
ungula::core::control::Pid pid(cfg);

void loop() {
    float error = setpoint - measurement;
    float output = pid.update(error, dt_s);  // dt_s must be > 0

    // Clamp output to safe range, then add base bias:
    float clamped = std::clamp(output + baseSpeed, minSpeed, maxSpeed);
    motor.setSpeed(clamped);
}
```

`reset()` clears integral and derivative history. Call it after a setpoint jump, when the loop has been disabled, or after a hard-stop (limit switch, motor disabled) — any case where the carried error state is no longer meaningful.

The derivative term stays at 0 on the first `update()` call so the first step doesn't produce a spurious spike.

Gains can be retuned at runtime without rebuilding the controller:

```cpp
ungula::core::control::PidConfig tuned = pid.config();  // current gains
tuned.kp = 1.4f;
pid.setConfig(tuned);
pid.reset();   // the integral survives setConfig() — clear it on a big jump
```

`integral()` exposes the accumulator if you want to log or display it.

`dt_s` is **not validated**. Passing `0` divides by zero in the derivative term and the output becomes `inf`/`NaN`, which then propagates into whatever you drive. If your `dt_s` comes from a tick delta, guard it at the call site.

## Utilities

### CRC32 (`ungula/core/util/crc32.h`)

```cpp
#include <ungula/core.h>

uint32_t checksum = ungula::core::util::crc32(data, len);
```

Standard polynomial `0xEDB88320`, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF`. No lookup table (saves RAM, costs 8 shifts per byte). `crc32_byte(crc, byte)` is the streaming step function. See the Preferences section below for a real-world usage example.

### Queue (`ungula/core/util/queue.h`)

Fixed-size circular queue, templated. Does not allocate on the heap. Lives in `ungula::core::util`.

```cpp
#include <ungula/core/util/queue.h>

ungula::core::util::Queue<int, 10> q;       // or: using ungula::core::util::Queue;
q.push(42);
int val;
q.pop(val);   // val = 42
q.peek(val);  // view without removing
q.count();    // number of items
q.isFull();
q.isEmpty();
q.clear();
```

### String Utilities (`ungula/core/util/string_utils.h`)

Namespace `ungula::core::util::str`. Everything is `inline`, header-only.

- In place: `trim`, `to_lower`, `to_upper`, `trimWhitespace(char*, size_t&)`, `replaceAll`.
- Returning a copy: `as_trim`, `as_lower`, `as_upper`, `escapeString`, `trimWhitespace(string_view_t)`, `skipWhitespace`, `skipWhitespaceView`.
- Inspecting: `startsWith`, `countChar`, `countTokensByChar`, `string_indexOf`, `string_equals`.
- Splitting/joining: `tokenizeByDelimiter`, `cleanDelimitedValues`, `string_substring`.
- Numbers: `num_to_string<T>`, `num_to_stringf<T>(num, buf, size)`.

```cpp
#include <ungula/core/util/string_utils.h>

ungula::core::util::string_t s = "  hello  ";
ungula::core::util::str::trim(s);                                  // "hello"
auto upper = ungula::core::util::str::as_upper(s);                 // "HELLO"
auto parts = ungula::core::util::str::tokenizeByDelimiter("a,b,c", ',');
```

### String types (`ungula/core/util/string_types.h`)

Project-wide aliases live in `ungula::core::util`: `string_t` (`std::string`), `string_view_t` (`std::string_view`), `vector_string_t` and `string_vector_t` (both `std::vector<string_t>` — same type, two spellings), `vector_string_view_t`. Code already inside `namespace ungula::core::util { ... }` finds them unqualified; everything else uses `ungula::core::util::string_t` etc.

### Temperature & Math (`ungula/core/util/types.h`)

Both nested under `ungula::core::util`.

```cpp
#include <ungula/core/util/types.h>

double f = ungula::core::util::temp::celsiusToFahrenheit(100.0);  // 212.0
double c = ungula::core::util::temp::fahrenheitToCelsius(600.0);  // 315.56
bool ok  = ungula::core::util::temp::isValidTemperature(300.0);   // true (finite && strictly inside -200 .. 1800)

int16_t wire = ungula::core::util::temp::packCelsius(25.5f);   // 255 — wire format
float recovered = ungula::core::util::temp::unpackCelsius(wire); // 25.5f

double v = ungula::core::util::math::clamp(1.5, 0.0, 1.0);   // 1.0
double t = ungula::core::util::math::lerp(0.0, 100.0, 0.5);  // 50.0

// In-place variants modify the value by reference:
double x = 1.5;
ungula::core::util::math::clamp_v(x, 0.0, 1.0);  // x == 1.0
```

`packCelsius` / `unpackCelsius` encode temperature as `int16_t` (celsius
× 10) for efficient wire transmission. `unpackCelsius` restores the
float. Both handle negative values correctly.

`clamp_v` modify the first argument in place.
`clamp` return the clamped value without touching the
input. `clamp01` and `lerp` are also available.

## Preferences (`ungula/core/preferences/`)

### Persistent Key-Value Storage

`IPreferences` lives at `ungula/core/preferences/i_preferences.h`, and
the facade `ungula/core/preferences/preferences.h` is the single include
for host projects. The facade also exposes a `Preferences` alias that
resolves to the platform's concrete implementation at compile time —
**use that alias from app code** so a future platform swap requires
zero source changes.

One implementation is currently provided:

- **`Esp32Preferences`** — uses the ESP-IDF `nvs_flash` API directly. No Arduino dependency. Header path: `ungula/core/preferences/platforms/esp32_preferences.h` (auto-included by `preferences.h` on ESP builds, where `Preferences` aliases to it).

The facade currently resolves as follows:

- `ESP_PLATFORM`, `ARDUINO_ARCH_ESP32`, or `ESP32` -> `Preferences = Esp32Preferences`
- `STM32` or `ARDUINO_ARCH_STM32` -> explicit compile-time `#error` (backend not implemented yet)
- anything else (typically a host gtest build) -> the `Preferences` alias is
  simply **not defined**. Any attempt to instantiate it host-side fails with an
  "undefined identifier" at the call site, which is the intent. `initStorage()`
  is still declared and resolves to the host no-op at link time.

Other implementations can be created under `ungula/core/preferences/platforms/` against the same interface, so your application code does not care which one is behind it.

### Mandatory storage init: `initStorage()`

```cpp
namespace ungula::core::preferences {
    bool initStorage();
}
```

For ESP32 call `initStorage()` exactly once at boot, **before** any other subsystem
that touches non-volatile storage — `Preferences`, `WiFi`, `ESP-NOW`,
`BLE`, and anything else that backs onto NVS. The function is
platform-agnostic at the call site; each backend supplies its own
implementation under `platforms/*.cpp`. On ESP32 it wraps
`nvs_flash_init()` and transparently erases-and-retries when the
on-flash format is stale (fresh chip, IDF version bump).

Call this function prior to `wifi`, `espnow`, `ble`, or any
`Preferences::begin()`; otherwise those calls fail with
`ESP_ERR_NVS_NOT_INITIALIZED` and the chip enters a reboot loop. Pure
ESP-IDF projects must invoke it from `app_main`-side setup. The host
build (gtest) provides a no-op default so the same source compiles and
links unmodified for unit tests.

```cpp
#include <ungula/core/preferences/preferences.h>

extern "C" void app_main()
{
    if (!ungula::core::preferences::initStorage()) {
        // storage backend refused to initialise even after the retry.
        abort();
    }
    // wifi::espnow_init(); transport.init(); Preferences::begin(...); etc.
}
```

### Per-instance API

```cpp
#include <ungula/core/preferences/preferences.h>
#include <emblogx/logger.h>

ungula::core::preferences::Preferences prefs;
// resolves to Esp32Preferences on ESP-IDF; future platforms plug in
// here without touching this line.

void saveDeviceConfig(const char* name, uint32_t bootCount) {
    prefs.begin("my_app");
    prefs.putString("name", name);
    prefs.putUInt32("boots", bootCount);
    prefs.end();
}

void loadDeviceConfig() {
    prefs.begin("my_app");
    char name[32];
    prefs.getString("name", name, sizeof(name));
    uint32_t boots = prefs.getUInt32("boots", 0);
    prefs.end();

    log_info("Device: %s, boots: %lu", name, boots);
}
```

Storing a struct with CRC validation:

```cpp
#include "ungula/core/util/crc32.h"

struct Settings {
    uint16_t speed;
    uint16_t temp;
    uint32_t crc;
};

void saveSettings(const Settings& s) {
    Settings copy = s;
    copy.crc = ungula::core::util::crc32(reinterpret_cast<const uint8_t*>(&copy), offsetof(Settings, crc));

    prefs.begin("settings");
    prefs.putBytes("data", reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
    prefs.end();
}

bool loadSettings(Settings& out) {
    prefs.begin("settings");
    size_t n = prefs.getBytes("data", reinterpret_cast<uint8_t*>(&out), sizeof(out));
    prefs.end();

    if (n != sizeof(out)) return false;
    uint32_t expected = ungula::core::util::crc32(reinterpret_cast<const uint8_t*>(&out), offsetof(Settings, crc));
    return out.crc == expected;
}
```

### Program Store (Recipe Manager)

`ProgramStore<ProgramT, MaxSlots>` is a generic NVS-backed recipe/program manager. It stores up to N slots of any POD struct, with CRC integrity checks, active/last-used index tracking, and auto-creation of a default program on first boot.

Path: `ungula/core/preferences/tools/programs/program_store.h`.

Your project defines the struct, the store handles persistence:

```cpp
#include <ungula/core/preferences/preferences.h>
#include <ungula/core/preferences/tools/programs/program_store.h>
#include <emblogx/logger.h>

// Project-specific recipe struct — any fields you need
struct MyRecipe {
    char name[32];
    int speed;
    int temperature;
    bool valid;  // required by ProgramStore

    static MyRecipe createDefault(const char* programName) {
        MyRecipe rec{};
        strncpy(rec.name, programName, sizeof(rec.name) - 1);
        rec.speed = 100;
        rec.temperature = 350;
        rec.valid = true;
        return rec;
    }
};

// 8 recipe slots, persisted in NVS namespace "programs"
ungula::core::preferences::programs::ProgramStore<MyRecipe, 8> store(prefs);

void setup() {
    store.init("DEFAULT", MyRecipe::createDefault);
    // Slot 0 now has a "DEFAULT" recipe if NVS was empty

    const auto& active = store.getProgram(store.getActiveIndex());
    log_info("Active: %s, speed=%d", active.name, active.speed);
}

void saveRecipe(int slot, const MyRecipe& recipe) {
    store.saveProgram(slot, recipe);  // persisted with CRC
}

void switchRecipe(int slot) {
    store.setActiveIndex(slot);  // persisted
}
```

Each slot is stored as `[struct bytes][CRC32]`. On load, CRC is verified — corrupted slots are silently skipped. The store guarantees at least one valid program exists at all times (won't delete the last one).

Two limits to keep in mind:

- Slot keys are `"p<index>"` in a 4-byte buffer, so keep `MaxSlots` **≤ 100** — index 100 truncates to `"p10"` and collides with slot 10.
- `activeIndex` / `lastUsedIndex` are persisted as `uint8_t`, with `255` reserved as the "no last-used slot" sentinel.

`saveProgram()` returns `true` once the in-RAM slot is updated; it does not surface a failed storage write. Read the slot back if you need write confirmation.

### Config Store (Single Config)

`NvsConfigStore<ConfigT>` stores one CRC-protected config struct in NVS under a single key.
As an example, the `ProgramStore` is used when managing an array of 'program settings' while `NvsConfigStore<ConfigT>` is used when you have one configuration structure only.

Path: `ungula/core/preferences/nvs_config_store.h`.

```cpp
#include <emblogx/logger.h>
#include <ungula/core/preferences/nvs_config_store.h>

struct MotorConfig {
    float kp = 1.0f;
    float ki = 0.1f;
    float kd = 0.05f;
    float max_rpm = 3000.0f;
};

static_assert(std::is_trivially_copyable<MotorConfig>::value, "NvsConfigStore requires trivially copyable ConfigT");

NvsConfigStore<MotorConfig> motorCfg(prefs, "motor", "config");

void loadMotorConfig() {
    ConfigLoadStatus status;
    MotorConfig cfg = motorCfg.load({}, &status);

    if (status == ConfigLoadStatus::Recovered) {
        log_warn("Motor config was corrupted, loaded defaults");
    } else if (status == ConfigLoadStatus::Defaulted) {
        log_info("No motor config stored, using defaults");
    }

    applyMotorConfig(cfg);
}

void saveMotorConfig(const MotorConfig& cfg) {
    motorCfg.save(cfg);
}
```

The blob written to NVS is `[ConfigT][CRC32]`. On a CRC mismatch at load, the defaults are returned AND rewritten — the corruption self-heals on the next boot.

### Counters (Named Lifetime Tallies)

`CounterStore` keeps named `uint32_t` counters in their own storage namespace — how many programs a machine has run, how many times it booted, how many cycles a component did. Each counter is one key, so writing one never rewrites the others.

Path: `ungula/core/preferences/tools/counters/counter_store.h`. Namespace: `ungula::core::preferences::counters`.

```cpp
#include <ungula/core/preferences/tools/counters/counter_store.h>

namespace counters = ungula::core::preferences::counters;

counters::CounterStore machineCounters(prefs);          // namespace "counters"
counters::CounterStore nodeCounters(prefs, "cnt_node"); // or your own

void onProgramStarted() {
    const uint32_t total = machineCounters.increment("runs"); // creates it at 1
    log_info("program run #%u", (unsigned)total);
}

uint32_t runsSoFar() {
    return machineCounters.read("runs");    // 0 when never created
}

void onFactoryReset() {
    machineCounters.reset("runs");          // back to 0, key kept
}
```

| Call | Behavior |
| --- | --- |
| `read(name)` | Current value. `0` when the counter does not exist. Never writes. |
| `write(name, value)` | Sets the value, creating the counter if needed. `false` on failure. |
| `increment(name, step = 1)` | Read + add + write. Returns the new value, or `0` if it failed. |
| `reset(name)` | `write(name, 0)`. The key stays, so `exists()` stays true. |
| `exists(name)` | Tells "never counted" from "counted zero times". |
| `isValidName(name)` (static) | 1..15 chars — validate operator-supplied names before use. |

Notes:

- **Name limit is 15 characters.** ESP32 NVS keys cap there; a longer name is rejected rather than truncated, because two names sharing the first 15 chars would silently become one counter.
- **Counters saturate at `UINT32_MAX`**, they do not wrap. That is also why a successful `increment()` can never return `0` — `0` means the write failed.
- **`CounterStore` holds no state**, only the `IPreferences` reference and the namespace. Build one at the call site if you prefer: `CounterStore(prefs).increment("runs");`
- **Flash wear**: one storage entry per write. On ESP32 NVS a 4 KB page holds 126 entries and is rewritten when full, against ~100k erase cycles per page — at 10 increments/day that is thousands of years. It is still a per-run/per-boot tool: never increment inside a control loop. Count in RAM and persist the total once, when the run finishes.
- **Not thread-safe.** The injected `IPreferences` owns one backend handle; call counters from the same task as your other preferences use.

## Testing

```shell
cd lib/tests
./1_build.sh
./2_run.sh
```

| Suite | What it covers |
| --- | --- |
| `CRC32` | Known values, empty input, single byte |
| `Queue` | Push/pop, overflow, underflow, wrap-around, clear, move |
| `StringUtils` | Trim, case, split, escape |
| `Types` | Temperature conversion and pack/unpack, math clamp/lerp, enum casts |
| `PID` | Proportional, integral, derivative, anti-windup, reset |
| `TimeControlTest` | Provider routing, timezone offsets, sync clock, `delayUntilMs`, formatters |
| `TimezoneTableTest` | Offsets, abbreviations, enum/table drift |
| `TimeFormatTest` | ISO-8601, custom specs, offsets, invalid inputs |
| `NvsConfigStore` | Load/save, default fallback, CRC recovery self-heal |
| `CounterStore` | Create-on-first-use, increment/reset, saturation, name limits, one write per increment, backend failure |
| `PreferencesLayout` | `ProgramStore` smoke test, `IPreferences` stays abstract, host `initStorage()` |

Tests build against the host backends (`time_control_host.h`,
`host_preferences.cpp`), so anything ESP-only — `SystemControl`,
`queryChipInfo()`, the FreeRTOS `delayUntil*` path — is not covered here.

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see the [LICENSE](LICENSE) file.

---

## Arduino CLI symlink note (rarely relevant)

This library ships a flat forwarder header at `src/ungula_core.h` that
just `#include`s `ungula/core.h`. `library.properties` `includes=` points
at the forwarder.

It only exists to work around an Arduino CLI quirk: when the library is
consumed through a symlink, the CLI sometimes fails to discover headers
nested under `src/ungula/`. The flat forwarder fixes that scan.

**Host code keeps including the real header**:

```cpp
#include <ungula/core.h>
```

PlatformIO, ESP-IDF component builds, and plain CMake setups can ignore
the forwarder.
