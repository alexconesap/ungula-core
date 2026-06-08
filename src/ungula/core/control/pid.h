// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// Generic single-loop PID controller.
// Output is unclamped: callers apply their own [min, max] clamp after adding
// their base speed / bias. The integral has its own anti-windup clamp (`i_max`)
// so a saturated output doesn't poison future ticks.
// -------------------------------------------------------------------

namespace ungula::core::control
{

struct PidConfig {
        float kp; // proportional gain
        float ki; // integral gain
        float kd; // derivative gain
        float i_max; // anti-windup clamp on |integral|
};

class Pid {
    public:
        explicit Pid(const PidConfig &cfg)
                : cfg_(cfg)
        {
        }

        /// Drop integral + derivative history. Call after a setpoint jump, when
        /// the loop has been disabled for a while, or after a hard-stop event
        /// (limit reached, motor disabled) — any case where the carried error
        /// state is no longer meaningful.
        void reset()
        {
                integral_ = 0.0f;
                prev_error_ = 0.0f;
                primed_ = false;
        }

        /// Run one iteration with the latest `error` (the setpoint − measurement
        /// sign convention is the caller's; the output sign follows). `dt_s` must
        /// be > 0. Returns the unclamped output `kp·e + ki·∫e + kd·de/dt`.
        float update(float error, float dt_s)
        {
                integral_ += error * dt_s;
                if (integral_ > cfg_.i_max) {
                        integral_ = cfg_.i_max;
                }
                if (integral_ < -cfg_.i_max) {
                        integral_ = -cfg_.i_max;
                }

                const float deriv = primed_ ? (error - prev_error_) / dt_s : 0.0f;
                prev_error_ = error;
                primed_ = true;

                return cfg_.kp * error + cfg_.ki * integral_ + cfg_.kd * deriv;
        }

        /// Current integral accumulator — useful when logging or tuning.
        float integral() const
        {
                return integral_;
        }

    private:
        PidConfig cfg_;
        float integral_ = 0.0f;
        float prev_error_ = 0.0f;
        bool primed_ = false; // false on first update → derivative stays 0
};

} // namespace ungula::core::control
