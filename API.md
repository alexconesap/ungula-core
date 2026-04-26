# UngulaCore (`lib`)

Portable C++17 core utilities for embedded firmware: a platform-abstracted
time/delay source, NVS-backed key-value preferences with CRC-checked
program slots, fixed-capacity queue, string helpers, CRC32, MCU
diagnostics, and a heap health monitor. ESP32 (ESP-IDF) is the fully
supported target; the portable subset (strings, queue, math, CRC,
time on host) builds for STM32/host with the bundled host backend.

The library is header-mostly. One umbrella header pulls in the public
surface for Arduino discovery; individual headers can also be included
directly when only part of the surface is needed.

---

## Usage

### Use case: Drift-free periodic loop

```cpp
#include <ungula_core.h>

using ungula::TimeControl;

void setup() {}

void loop() {
    auto ref = TimeControl::millis();
    while (true) {
        // ... read sensors, push status ...
        TimeControl::delayUntilMs(ref, 50);   // every 50 ms, no drift
    }
}
```

When to use this: any periodic task (sensor poll, heartbeat, control
loop). `delayUntilMs` advances `ref` by the period, so jitter from work
inside the loop does not accumulate.

### Use case: Microsecond-grade sleep

```cpp
#include <time/time_control.h>

ungula::TimeControl::delayUs(250);   // ~250 µs busy-wait on ESP32
```

When to use this: bit-banging, short hardware-timing windows. Prefer
`delayMs` for anything ≥ 1 ms — it yields to FreeRTOS.

### Use case: Wall-clock time via a pluggable provider

```cpp
#include <time/time_control.h>
#include <time/i_time_provider.h>

class NtpClock : public ungula::ITimeProvider {
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
    ungula::TimeControl::setTimeProvider(&clock);
    ungula::TimeControl::setTimezone(ungula::tz::Timezone::CET);
}

void printNow() {
    char buf[20];
    if (ungula::TimeControl::formatLocal(buf, sizeof(buf)) > 0) {
        // buf == "2026-04-26 14:32:01"
    }
}
```

When to use this: any code that needs a real wall-clock instant
(logging timestamps, scheduled tasks). Without a provider, `now()`
falls back to monotonic-since-boot.

### Use case: Persisted key-value preferences (ESP32)

```cpp
#include <preferences/core/esp32_preferences.h>

ungula::Esp32Preferences prefs;

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
#include <preferences/core/esp32_preferences.h>
#include <preferences/programs/program_store.h>

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

ungula::Esp32Preferences           prefs;
ungula::ProgramStore<Recipe, 10>   store(prefs);

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

### Use case: Fixed-capacity queue (no heap)

```cpp
#include <util/queue.h>

ungula::Queue<int, 16> q;

void onIrq(int sample) {
    q.push(sample);          // false if full — caller decides
}

void drain() {
    int v;
    while (q.pop(v)) {
        // process
    }
}
```

When to use this: producer/consumer between an ISR and a task, or any
bounded buffer where heap allocation is forbidden.

### Use case: String helpers

```cpp
#include <util/string_utils.h>

using ungula::string_t;
using namespace ungula::str;

string_t s = "  Hello, World  ";
trim(s);                                     // "Hello, World"
to_lower(s);                                 // "hello, world"
int n = replaceAll(s, "world", "ungula");    // 1
auto parts = tokenizeByDelimiter(s, ',');    // ["hello", " ungula"]
```

### Use case: CRC32 of a buffer

```cpp
#include <util/crc32.h>

uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
uint32_t crc = ungula::crc32(payload, sizeof(payload));
```

### Use case: Reboot the MCU

```cpp
#include <system/system_reboot.h>

ungula::SystemControl::reboot();              // immediate
ungula::SystemControl::rebootAfterMs(2000);   // give logs time to flush
```

### Use case: Heap-leak watchdog

```cpp
#include <system/health_monitor.h>

static ungula::HealthMonitor health;

void loop() {
    ungula::HealthSample s;
    if (health.sample(60'000, s)) {
        // emit s.free_heap, s.min_free_heap, s.delta, s.uptime_ms
    }
}
```

When to use this: long-running firmware. A monotonically falling
`free_heap` is the only early signal of an allocator leak.

### Use case: Identify the MCU at boot

```cpp
#include <system/chip_info.h>

void printBootBanner() {
    ungula::ChipInfo info = ungula::queryChipInfo();
    // info.model, info.sdkVersion, info.cores, info.hasWifi, ...
}
```

---

## Public types

| Type | Header | Purpose |
| ---- | ------ | ------- |
| `ungula::TimeControl` | `time/time_control.h` | Static-only time/delay facade |
| `ungula::ITimeProvider` | `time/i_time_provider.h` | Pluggable wall-clock source |
| `ungula::tz::Timezone` (enum) | `time/timezones.h` | Named UTC offset codes |
| `ungula::IPreferences` | `preferences/core/i_preferences.h` | Abstract NVS interface |
| `ungula::Esp32Preferences` | `preferences/core/esp32_preferences.h` | ESP-IDF NVS implementation |
| `ungula::ProgramStore<T, N>` | `preferences/programs/program_store.h` | CRC-checked recipe slot table |
| `ungula::Queue<T, Capacity>` | `util/queue.h` | Fixed-capacity circular queue |
| `ungula::SystemControl` | `system/system_reboot.h` | Reboot helpers |
| `ungula::HealthMonitor` / `HealthSample` | `system/health_monitor.h` | Heap sampler |
| `ungula::ChipInfo` | `system/chip_info.h` | MCU identity struct |
| `ungula::string_t`, `string_view_t`, `vector_string_t` | `util/string_types.h` | std-aliases used across all libraries |

Time-aliases inside `TimeControl`: `tick_ms_t`, `tick_us_t`,
`duration_ms_t`, `duration_us_t`, `epoch_ms_t` — all `int64_t`. Names
exist to make intent visible at call sites; types are interchangeable.

`TimeControl`, `SystemControl` are non-instantiable (constructors
deleted). All members are `static`.

---

## Public functions / methods

### `ungula::TimeControl` (selected)

- **`static tick_ms_t millis()`** / **`static tick_us_t micros()`**
  Monotonic since boot. Both `int64_t` — never wrap in any device
  lifetime.
- **`static void delay(duration_ms_t)`** / **`delayMs`** / **`delayUs`**
  Block the current task. ESP32 backend yields via FreeRTOS for
  `delayMs`; `delayUs` busy-waits.
- **`static void delayUntilMs(tick_ms_t& ref, duration_ms_t period)`**
  Wait until `ref + period`, then advance `ref` by `period`. Drift-free.
  Identical signature in `delayUntilUs`.
- **`static void yield()`** — alias for `delayMs(0)`.
- **`static epoch_ms_t now() / nowUtc() / nowLocal()`**
  Wall-clock if a provider is installed and valid; otherwise
  monotonic-since-boot. `nowLocal()` adds the configured offset.
- **`static epoch_ms_t nowInTz(int32_t offsetSeconds)`**
  One-shot conversion without touching the stored offset.
- **`static void setTimeProvider(ITimeProvider*)`** /
  **`clearTimeProvider()`** — install/remove the wall-clock source.
- **`static void setTimezone(tz::Timezone)`** /
  **`setTimezoneOffsetSeconds(int32_t)`** /
  **`int32_t timezoneOffsetSeconds()`**
- **`static size_t formatUtc(char*, size_t)`** /
  **`formatLocal(char*, size_t)`** /
  **`format(char*, size_t, const char* strftimeFmt)`**
  All return `0` when no valid provider is installed (formatting a
  monotonic tick as a date would print "1970-…" otherwise).
- **`static void setSyncTime(tick_ms_t remoteMs)`** /
  **`setSyncTimeUs`** / **`tick_ms_t syncNow()`** /
  **`tick_us_t syncNowUs()`** / **`int64_t syncOffset()`** /
  **`bool isSynced()`** / **`clearSync()`**
  Network-coordinator clock alignment. `setSyncTime` stores a single
  offset; reads add it on the hot path.

### `ungula::IPreferences` / `Esp32Preferences`

- **`bool begin(const char* ns)`** — open NVS namespace (≤ 15 chars).
  Must succeed before any get/put.
- **`void end()`** — close namespace; pair with each `begin`.
- **`bool putString / putBytes / putUInt8 / putUInt32`** — write.
- **`size_t getString(key, buf, bufSize)`** /
  **`getBytes(key, buf, bufSize)`** — return bytes read, `0` if missing.
- **`uint8_t / uint32_t getUInt8/32(key, defaultVal)`** — typed reads.
- **`bool remove(key)`**, **`bool clear()`**, **`bool hasKey(key)`**.

`Esp32Preferences` is the only concrete shipped implementation; it
compiles only when `ESP_PLATFORM` is defined. Inject `IPreferences&`
into code that needs to be host-testable.

### `ungula::ProgramStore<ProgramT, MaxPrograms>`

`ProgramT` must be POD with `char name[N]` and `bool valid` fields.

- **`explicit ProgramStore(IPreferences&)`** — borrow-only reference.
- **`void init(const char* defaultName, ProgramT (*createDefault)(const char*))`**
  Load all slots, validate CRCs, auto-create slot 0 from
  `createDefault` if no valid program exists.
- **`const ProgramT& getProgram(int index)`** — clamps to slot 0 on
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

### `ungula::Queue<T, Capacity>`

- **`bool push(const T&)`** / **`bool push(T&&)`** — `false` when full.
- **`bool pop(T& out)`** — `false` when empty; moves into `out`.
- **`bool peek(T& out) const`** — copy of the front element.
- **`size_t count()`**, **`bool isFull()`**, **`bool isEmpty()`**,
  **`constexpr size_t capacity()`**, **`void clear()`**.

`noexcept` throughout. Storage is a `T data_[Capacity]` member — no
heap. Not thread-safe; wrap with a mutex or use one queue per
producer/consumer pair.

### `ungula::str` (selected, namespace `ungula::str`)

`trim(string_t&)`, `as_trim`, `to_lower`, `as_lower`, `to_upper`,
`as_upper`, `startsWith`, `replaceAll`, `escapeString`, `countChar`,
`countTokensByChar`, `tokenizeByDelimiter`, `cleanDelimitedValues`,
`string_indexOf`, `string_substring`, `string_equals`,
`num_to_string<T>`, `num_to_stringf<T>`, `skipWhitespace*`,
`trimWhitespace`. All inline.

### `ungula::` math/temperature helpers (in `util/types.h`)

- `math::clamp / clampf / clampi / clamp01 / clamp01f / lerp / lerpf`.
- `temperature::celsiusToFahrenheit / fahrenheitToCelsius` (double and
  float variants), `isValidTemperature(C)` returns false for non-finite
  or values outside `(-200°C, 1800°C)`.
- `toUint8<E>()`, `fromUint8<E>(uint8_t)` — generic enum conversion.

`util/types.h` also defines a set of legacy `stringToInt`,
`stringIndexOf`, `intToString`, `stringTrim` helpers in the `ungula::`
namespace. Prefer the `ungula::str::` versions for new code; the
free-function set exists for porting Arduino code with minimal edits.

### CRC32

- **`uint32_t ungula::crc32(const uint8_t* data, size_t len)`**
- **`uint32_t ungula::crc32_byte(uint32_t crc, uint8_t byte)`** —
  step function for streaming.

Polynomial `0xEDB88320` (zlib/Ethernet). Initial `0xFFFFFFFF`,
final XOR `0xFFFFFFFF`.

### `ungula::SystemControl`

- **`static void reboot()`** — immediate hard reset.
- **`static void rebootAfterMs(uint32_t)`** — sleeps then reboots.

### `ungula::HealthMonitor`

- **`bool sample(uint32_t intervalMs, HealthSample& out)`** — fills
  `out` and returns `true` only when at least `intervalMs` have passed
  since the previous sample. The first call always returns `true` with
  `delta == 0`.
- **`void reset()`** — clear baseline (use after a planned big
  alloc/free).

### `ungula::queryChipInfo()`

Returns a `ChipInfo` populated by the platform backend. Strings are
fixed-size in-struct buffers, so the value can be stored or copied
freely.

### `ungula::tz`

- **`enum class Timezone : uint8_t`** — ~30 entries (UTC, GMT, CET,
  EST, JST, …). Disambiguated suffixes for clashes (`CST_NA`,
  `CST_CN`, `IST_IN`).
- **`constexpr int32_t tz::offsetSeconds(Timezone)`** — fixed offset
  from UTC. No DST awareness. Enum values are stable across versions
  (entries can be added at the end only).

---

## Lifecycle

- **TimeControl** — no init required for `millis`/`micros`/`delay*`.
  For `now()` to return wall-clock, install a provider with
  `setTimeProvider` and ensure `isValid()` returns `true` before the
  first call. `setTimezone` is a one-shot configuration.
- **Esp32Preferences** — every read/write must be sandwiched in
  `begin(ns)` … `end()`. NVS init (`nvs_flash_init`) must happen
  before the first `begin`. Forgetting `end()` leaks an NVS handle.
- **ProgramStore** — call `init()` exactly once in `setup()`. Must run
  after the underlying `IPreferences` is usable. `saveProgram` and
  `deleteProgram` persist immediately; metadata (`activeIndex`,
  `lastUsedIndex`) is also persisted on every change.
- **HealthMonitor** — single instance per project, sampled from
  `loop()`. No init required.
- **Queue** — value-initialized; no init required.

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
- `TimeControl::format*` — return `0` when no valid time provider is
  installed (caller must check before treating the buffer as printable).
- `tz::offsetSeconds` — undefined enum values return `0` (UTC).

---

## Threading / timing / hardware notes

- **TimeControl::millis / micros**: ESP32 backend uses
  `esp_timer_get_time()` — ISR-safe and lock-free. Host backend uses
  `std::chrono::steady_clock`.
- **TimeControl::delayMs**: ESP32 yields via FreeRTOS
  (`vTaskDelay`-equivalent). Host backend uses
  `std::this_thread::sleep_for`. Do NOT call from inside an ISR.
- **TimeControl::delayUs**: busy-wait on ESP32 — does not yield.
  Acceptable up to a few hundred microseconds.
- **TimeControl static state** (`provider_`, `sync_`,
  `timezoneOffsetSeconds_`): not protected by a mutex. Configure once
  during `setup()` from a single task; readers can be concurrent.
- **Queue**: not thread-safe. One producer + one consumer is fine
  only if you accept the standard SPSC caveats; for ISR↔task use a
  FreeRTOS queue or wrap with `portENTER_CRITICAL`.
- **NVS access** is mutex-protected by ESP-IDF, but `Esp32Preferences`
  is single-instance-per-namespace by design — do not share one
  `Esp32Preferences` object across tasks.
- **CRC32** functions are pure and reentrant.
- **HealthMonitor::sample** reads
  `esp_get_free_heap_size`/`esp_get_minimum_free_heap_size` on ESP32 —
  cheap, but not zero-cost; the interval gate exists for that reason.

---

## Internals not part of the public API

- `time/platforms/time_control_esp32.h`,
  `time/platforms/time_control_host.h` — picked automatically by
  `time_control.h`. Never `#include` directly.
- `TimeControl::hasReachedMs / hasReachedUs` — private comparison
  helpers.
- `TimeControl::SyncState` and the static storage members
  (`provider_`, `sync_`, `timezoneOffsetSeconds_`) — implementation
  detail of the static facade.
- `ProgramStore::ProgramBlob`, `computeCrc`, `loadAllFromNvs`,
  `saveProgramToNvs`, `loadMetaFromNvs`, `saveMetaToNvs`,
  `programKey`, `NVS_NS = "programs"` — internal layout. Do not
  reach into NVS for these keys directly.
- `preferences/core/esp32_preferences.cpp` — the `nvs_flash` glue.
  Use the `IPreferences` interface; never include this file from app
  code.
- The `ungula::platformMillis()` / `ungula::platformMicros()` helpers
  in `util/types.h` are legacy 32-bit shims kept for source
  compatibility with old code; new code uses `TimeControl::millis()`.

---

## LLM usage rules

- Use `TimeControl` for all time and delay needs. Never call `millis()`,
  `micros()`, `delay()`, or platform timer APIs (`esp_timer_*`,
  `vTaskDelay`) directly from project code.
- Use `delayUntilMs` for periodic loops; `delayMs` only for fire-and-
  forget waits.
- Treat `IPreferences` as the only persistence interface; depend on the
  abstract type, not on `Esp32Preferences`. Single-namespace per object.
- For recipe-style state, prefer `ProgramStore` over hand-rolled NVS
  serialization — CRC and slot management are non-trivial to redo.
- Use `Queue<T, N>` instead of `std::deque` / dynamic ring buffers for
  bounded buffers. No heap is allocated after construction.
- Use `ungula::string_t` / `string_view_t` aliases in new code rather
  than `String` (Arduino) or raw `std::string`. Use `ungula::str::`
  helpers; the `stringFoo` free functions in `util/types.h` are a
  porting aid and should not be the destination for new code.
- All time values flowing through public APIs are `int64_t`. Don't
  truncate to `uint32_t` "to save space" — past 49 days it wraps.
- Don't include `time/platforms/*` headers; let `time_control.h`
  dispatch.
- Don't read or write the `programs` NVS namespace by hand — that's
  `ProgramStore`'s territory.
- Don't add logging into this library; surface state via return values
  or callbacks (project rule).
- The umbrella header `<ungula_core.h>` is the Arduino-CLI discovery
  hook; in non-Arduino builds, including the specific `time/...`,
  `preferences/...`, `util/...`, `system/...` header is equivalent.
