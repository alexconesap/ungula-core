# UngulaCore

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — core utilities (time, preferences, CRC, queues, strings).

Generic utility library for embedded projects, fully portable. It handles persistent storage, time control/delays, logging, system control, and various utility functions.

When I recently started working on an existing Arduino-based C++ project that used only the ESP32 hardware, I quickly realized how much work it takes to migrate from Arduino libraries to the ESP32 SDK. To do this gradually, I began implementing this library, which allows me to easily port projects that still rely on certain Arduino libraries (for example, to control specific motor drivers or sensors) while accessing ESP32 libraries natively to improve overall performance.

Ultimately, my goal is to port 100% of the code from one hardware platform for example STM to another one such as ESP with minimal impact, keeping the system fully abstracted and introducing minimal overhead. A key objective is also to eliminate Arduino libraries that are not optimized for any specific platform.

## Quick Start

In your Arduino `.ino` file:

```cpp
#include <ungula_core.h>
```

Then include whatever component you need in your code:

```cpp
#include "preferences/core/i_preferences.h"
#include "util/string_utils.h"
#include "util/queue.h"
#include "util/crc32.h"
#include "time/time_control.h"
#include "system/system_reboot.h"
...
```

## Time Control (`time/`)

Portable time abstraction. All static methods, no instantiation needed. `time_control.h` is the only header your code ever includes; it dispatches to a platform-specific backend at build time:

```text
time/
  time_control.h                       # public API (unchanged)
  i_time_provider.h                    # pluggable clock source
  platforms/
    time_control_esp32.h               # ESP-IDF: FreeRTOS + esp_timer
    time_control_host.h                # std::chrono + std::thread (tests, STM32 fallback)
```

Selection happens at the bottom of `time_control.h` via a single `#if defined(ESP_PLATFORM)`. Adding a new platform (STM32, etc.) means dropping one more file into `platforms/` and adding one `#elif` branch — no cross-platform `#ifdef` clutter inside any implementation file.

### Periodic Loop Without Drift

This is the main use case. Instead of doing `delay(10)` at the end of a loop (which accumulates drift from the work time), use `delayUntilMs`. It advances the reference by the period, not by "now + period", so you get consistent timing:

```cpp
#include "time/time_control.h"

using ungula::TimeControl;

auto ref = TimeControl::millis();
while (running) {
    readSensors();  // takes 3ms
    sendStatus();   // takes 4ms

    // delay(50) would wait 50ms on top of the 7ms of work = 57ms total.
    // delayUntilMs waits only 43ms — the time remaining to reach 50ms.
    TimeControl::delayUntilMs(ref, 50);  // 50ms period, drift-free
}
```

Same thing in microseconds (best-effort, busy-wait):

```cpp
auto ref = TimeControl::micros();
while (stepping) {
    toggleStepPin();
    TimeControl::delayUntilUs(ref, 200);  // 200us period
}
```

### Simple Delays and cross-platform time functions

```cpp
TimeControl::delayMs(100);           // block for 100ms
TimeControl::delayUs(500);           // block for 500us (busy-wait)
auto now = TimeControl::millis();    // monotonic ms tick
auto us  = TimeControl::micros();    // monotonic us tick
```

### Useful helpers

```cpp
// Stop using 
TimeControl::delayMs(0); // It works but requires a side commentary
// Another dev could remove it, or waste time trying to figure out
// if 'is 0 wrong? Missing a 1 in front to be 10?'

// You can express better your intent by using
TimeControl::yield();
```

### Pluggable time source (`time/i_time_provider.h`)

`TimeControl::millis()` is always the local monotonic clock. `TimeControl::now()` can be routed through a custom source by installing an `ITimeProvider`. Typical uses: an NTP-synced wall-clock, an external RTC chip, a mock clock in tests.

```cpp
#include "time/i_time_provider.h"
#include "time/time_control.h"

using ungula::ITimeProvider;
using ungula::TimeControl;

/// Real-world example: a provider fed by the NTP sink elsewhere in the
/// firmware. Reports a current epoch time when synchronised, and falls
/// back to "invalid" until the first successful sync — at which point
/// TimeControl::now() returns the local monotonic clock on its own.
class NtpTimeProvider final : public ITimeProvider {
    public:
        uint32_t nowMs() const override {
            return current_epoch_ms_;
        }
        bool isValid() const override {
            return synchronized_;
        }

        /// Called by the NTP sink whenever a fresh sample lands.
        void onNtpSample(uint32_t epochMs) {
            current_epoch_ms_ = epochMs;
            synchronized_ = true;
        }

        /// Called when the link drops or the sample goes stale.
        void invalidate() {
            synchronized_ = false;
        }

    private:
        uint32_t current_epoch_ms_ = 0;
        bool synchronized_ = false;
};

NtpTimeProvider g_ntpProvider;

void setup() {
    TimeControl::setTimeProvider(&g_ntpProvider);
    // now() returns local millis() until the first NTP sample lands.
}
```

Contract:

- The provider must outlive the `setTimeProvider()` call. `TimeControl` stores the pointer, it does not copy.
- `isValid()` is checked on **every** `now()` call — not cached — so a provider can toggle its validity at runtime without re-registering.
- When `isValid()` returns false, `TimeControl::now()` falls back to the local monotonic clock. No exception, no "frozen" value.
- `clearTimeProvider()` removes the provider and restores local-clock behaviour.
- `micros()` and `nowUs()` are **not** routed through the provider — microsecond-grade external sources are rare enough to not pay for the indirection.

`ITimeProvider` is independent from the `setSyncTime()` / `syncNow()` pair. The sync clock stores a fixed offset to a coordinator's millisecond timestamp; the provider replaces the clock source entirely. Pick one, not both, per deployment.

## System Control (`system/`)

```cpp
using ungula::SystemControl;

SystemControl::reboot();
SystemControl::rebootAfterMs(500);  // wait 500ms then reboot
```

### Chip Info (`system/chip_info.h`)

Query the MCU at boot to log hardware details. The header is portable (plain struct, no SDK types). The implementation calls ESP-IDF internally.

```cpp
#include <system/chip_info.h>

void setup() {
    ungula::ChipInfo chip = ungula::queryChipInfo();
    log_info("MCU: %s rev%d, %d cores, IDF %s",
             chip.model, chip.revision, chip.cores, chip.sdkVersion);
    log_info("Features: %s", chip.features);

    if (chip.hasPsram) {
        log_info("PSRAM available");
    }
}
```

Fields: `model`, `sdkVersion`, `features` (human-readable string), `cores`, `revision`, `hasWifi`, `hasBluetooth`, `hasBle`, `hasPsram`.

## Utilities

### CRC32 (`util/crc32.h`)

```cpp
uint32_t checksum = ungula::crc32(data, len);
```

Standard polynomial `0xEDB88320`. No lookup table (saves RAM). See the Preferences section above for a real-world usage example.

### Queue (`util/queue.h`)

Fixed-size circular queue, templated. Does not allocate on the heap.

```cpp
Queue<int, 10> q;
q.push(42);
int val;
q.pop(val);   // val = 42
q.peek(val);  // view without removing
q.count();    // number of items
q.isFull();
q.isEmpty();
q.clear();
```

### String Utilities (`util/string_utils.h`)

Namespace `su::`. Various string manipulation helpers: `trim`, `to_lower`, `to_upper`, `startsWith`, `replaceAll`, `tokenizeByDelimiter`, `escapeString`, `countChar`, `num_to_string`.

### Temperature & Math (`util/types.h`)

```cpp
double f = temperature::celsiusToFahrenheit(100.0);  // 212.0
double c = temperature::fahrenheitToCelsius(600.0);   // 315.56
bool ok = temperature::isValidTemperature(300.0);     // true (finite && [-200, 1800))

double v = math::clamp(1.5, 0.0, 1.0);  // 1.0
double t = math::lerp(0.0, 100.0, 0.5); // 50.0
```

## Preferences (`preferences/`)

### Persistent Key-Value Storage

`IPreferences` abstracts NVS (or any other key-value store). One implementation is provided:

- **`Esp32Preferences`** — uses the ESP-IDF `nvs_flash` API directly. No Arduino dependency.

Other implementations can be created against the same interface, so your application code does not care which one is behind it.

```cpp
#include "preferences/esp32_preferences.h"

ungula::Esp32Preferences prefs;
// or: any other implementation you can create

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
#include "util/crc32.h"

struct Settings {
    uint16_t speed;
    uint16_t temp;
    uint32_t crc;
};

void saveSettings(const Settings& s) {
    Settings copy = s;
    copy.crc = ungula::crc32(reinterpret_cast<const uint8_t*>(&copy), offsetof(Settings, crc));

    prefs.begin("settings");
    prefs.putBytes("data", reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
    prefs.end();
}

bool loadSettings(Settings& out) {
    prefs.begin("settings");
    size_t n = prefs.getBytes("data", reinterpret_cast<uint8_t*>(&out), sizeof(out));
    prefs.end();

    if (n != sizeof(out)) return false;
    uint32_t expected = ungula::crc32(reinterpret_cast<const uint8_t*>(&out), offsetof(Settings, crc));
    return out.crc == expected;
}
```

### Program Store (Recipe Manager)

`ProgramStore<ProgramT, MaxSlots>` is a generic NVS-backed recipe/program manager. It stores up to N slots of any POD struct, with CRC integrity checks, active/last-used index tracking, and auto-creation of a default program on first boot.

Your project defines the struct, the store handles persistence:

```cpp
#include <preferences/programs/program_store.h>

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
ungula::ProgramStore<MyRecipe, 8> store(prefs);

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

## Testing

```shell
cd lib/tests
./1_build.sh
./2_run.sh
```

| Suite | What it covers |
| --- | --- |
| `CRC32` | Known values, empty input, single byte |
| `Queue` | Push/pop, overflow, underflow, clear |
| `StringUtils` | Trim, case, split, escape |
| `Types` | Temperature conversion, math clamp/lerp |

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](license.txt) file.
