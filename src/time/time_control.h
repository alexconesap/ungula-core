// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

namespace ungula {

    // Portable time abstraction for embedded targets.
    //
    // All methods are static — this is a utility class, you don't instantiate it.
    // Tick types use unsigned long to match Arduino millis()/micros() directly.
    // On ESP32 and Arduino (AVR, ARM) this is 32-bit, giving ~49 day ms wrap
    // and ~71 minute us wrap. Wrap-around is handled with signed-delta arithmetic.
    //
    // Typical usage for a periodic loop without drift:
    //
    //   auto ref = TimeControl::millis();
    //   while (true) {
    //       doWork();
    //       TimeControl::delayUntilMs(ref, 10);  // 10ms period, no drift
    //   }
    //
    // For one-shot delays, just use delayMs() or delayUs().
    //
    // Network time sync:
    //   A coordinator broadcasts its now() to child nodes. Each child calls
    //   setSyncTime() once; from then on syncNow() returns time aligned with
    //   the coordinator's clock. If no sync has been set, syncNow() == now().
    //
    //   // Coordinator sends its timestamp:
    //   msg.timestamp = TimeControl::now();
    //
    //   // Child receives and injects:
    //   TimeControl::setSyncTime(msg.timestamp);
    //
    //   // All nodes read the same logical clock:
    //   auto coordTime = TimeControl::syncNow();
    //
    // Platform support: ESP32 (FreeRTOS) and generic Arduino.
    // On ESP32, delayMs/delayUntilMs use vTaskDelay so other tasks can run.
    // On plain Arduino, they busy-wait.

    class TimeControl final {
        public:
            using ms_tick_t = uint32_t;
            using us_tick_t = uint32_t;
            using time_ms_t = uint32_t;
            using time_us_t = uint32_t;

            TimeControl() = delete;
            ~TimeControl() = delete;

            TimeControl(const TimeControl&) = delete;
            TimeControl& operator=(const TimeControl&) = delete;

            // ---- Local clock ----

            /// Monotonic millisecond tick. Wraps around every ~49 days.
            static ms_tick_t millis();

            /// Monotonic microsecond tick. Wraps around every ~71 minutes.
            static us_tick_t micros();

            /// Alias for millis().
            static ms_tick_t now() {
                return millis();
            }

            /// Alias for micros().
            static us_tick_t nowUs() {
                return micros();
            }

            // ---- Network-synced clock ----
            // Stores offset between local clock and a remote coordinator's clock.
            // setSyncTime() is called once per sync event (cheap: one subtraction).
            // syncNow() is called on every read (cheap: one addition).

            /// Inject a coordinator's millisecond timestamp. Computes and stores
            /// the offset between local and remote clocks.
            static void setSyncTime(ms_tick_t remoteMs) {
                sync_.offsetMs = static_cast<int32_t>(remoteMs - millis());
                sync_.active = true;
            }

            /// Inject a coordinator's microsecond timestamp (64-bit for precision).
            static void setSyncTimeUs(int64_t remoteUs) {
                sync_.offsetUs = remoteUs - static_cast<int64_t>(micros());
                sync_.active = true;
            }

            /// Coordinator-aligned millisecond time. Returns local millis() if
            /// no sync has been set.
            static ms_tick_t syncNow() {
                return millis() + static_cast<ms_tick_t>(sync_.offsetMs);
            }

            /// Coordinator-aligned microsecond time (64-bit).
            static int64_t syncNowUs() {
                return static_cast<int64_t>(micros()) + sync_.offsetUs;
            }

            /// Millisecond offset between local and remote clock. 0 if no sync.
            static int32_t syncOffset() {
                return sync_.offsetMs;
            }

            /// Microsecond offset between local and remote clock. 0 if no sync.
            static int64_t syncOffsetUs() {
                return sync_.offsetUs;
            }

            /// True if setSyncTime() or setSyncTimeUs() has been called.
            static bool isSynced() {
                return sync_.active;
            }

            /// Clear sync state — syncNow() reverts to local clock.
            static void clearSync() {
                sync_.offsetMs = 0;
                sync_.offsetUs = 0;
                sync_.active = false;
            }

            // ---- Delays ----

            /// Simple wrapper for delay in milliseconds. On ESP32 this yields
            /// to FreeRTOS scheduler. On Arduino it busy-waits.
            static void delay(time_ms_t msv) {
                delayMs(msv);
            }

            /// Blocking delay in milliseconds.
            static void delayMs(time_ms_t msv);

            /// Blocking delay in microseconds. Always busy-waits.
            static void delayUs(time_us_t usv);

            /// Wait until the next periodic boundary, then advance 'reference'
            /// by periodMs. Avoids drift because reference is bumped by the
            /// period, not reset to now.
            ///
            /// Example — run something every 50ms:
            ///   auto ref = TimeControl::millis();
            ///   while (running) {
            ///       readSensors();
            ///       sendStatus();
            ///       TimeControl::delayUntilMs(ref, 50);
            ///   }
            static void delayUntilMs(ms_tick_t& reference, time_ms_t periodMs);

            /// Same idea but in microseconds. Best-effort — relies on busy-wait.
            static void delayUntilUs(us_tick_t& reference, time_us_t periodUs);

            /// Cooperative yield — delay for 0 ms to let other tasks run. No-op on plain Arduino.
            static void yield() {
                delayMs(0);
            }

        private:
            static bool hasReachedMs(ms_tick_t now, ms_tick_t target);
            static bool hasReachedUs(us_tick_t now, us_tick_t target);

            /// Sync state — offset between local and coordinator clocks.
            /// Written once per sync event, read on every syncNow() call.
            struct SyncState {
                    int32_t offsetMs;
                    int64_t offsetUs;
                    bool active;
            };

            static SyncState sync_;
    };

}  // namespace ungula
