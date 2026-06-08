#include <gtest/gtest.h>
#include <ungula/core/control/pid.h>

using ungula::core::control::Pid;
using ungula::core::control::PidConfig;

TEST(PID, POnly)
{
        PidConfig cfg{ .kp = 2.0f, .ki = 0.0f, .kd = 0.0f, .i_max = 100.0f };
        Pid pid(cfg);

        float out = pid.update(5.0f, 0.01f);
        EXPECT_EQ(out, 10.0f);  // kp * error = 2.0 * 5.0
}

TEST(PID, IOnlyAccumulates)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 1.0f, .kd = 0.0f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(10.0f, 0.1f);  // integral += 10 * 0.1 = 1.0
        pid.update(10.0f, 0.1f);  // integral += 10 * 0.1 = 2.0
        pid.update(10.0f, 0.1f);  // integral += 10 * 0.1 = 3.0

        float out = pid.update(10.0f, 0.1f);  // integral = 4.0, ki * integral = 4.0
        EXPECT_FLOAT_EQ(out, 4.0f);
}

TEST(PID, IAntiWindupClamp)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 1.0f, .kd = 0.0f, .i_max = 2.0f };
        Pid pid(cfg);

        pid.update(100.0f, 0.1f);  // integral += 10 -> 10, clamped to 2
        EXPECT_FLOAT_EQ(pid.integral(), 2.0f);

        pid.update(100.0f, 0.1f);  // integral would be 12, stays at 2
        EXPECT_FLOAT_EQ(pid.integral(), 2.0f);

        float out = pid.update(100.0f, 0.1f);  // ki * 2 = 2
        EXPECT_FLOAT_EQ(out, 2.0f);
}

TEST(PID, DOnlyFirstCallIsZero)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 0.0f, .kd = 1.0f, .i_max = 100.0f };
        Pid pid(cfg);

        float out1 = pid.update(10.0f, 0.1f);  // first call: deriv = 0
        EXPECT_FLOAT_EQ(out1, 0.0f);

        float out2 = pid.update(10.0f, 0.1f);  // second call: deriv = (10-10)/0.1 = 0
        EXPECT_FLOAT_EQ(out2, 0.0f);
}

TEST(PID, DOnlyWithErrorChange)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 0.0f, .kd = 2.0f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(0.0f, 0.1f);   // primed_ = false, deriv = 0, primed_ = true
        float out = pid.update(5.0f, 0.1f);  // deriv = (5-0)/0.1 = 50, kd * 50 = 100
        EXPECT_FLOAT_EQ(out, 100.0f);
}

TEST(PID, CombinedOutput)
{
        PidConfig cfg{ .kp = 1.0f, .ki = 1.0f, .kd = 1.0f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(0.0f, 0.1f);  // first call: P=0, I=0, D=0
        // integral_ = 0

        // error=10, dt=0.1
        // integral = min(10*0.1, 100) = 1
        // deriv = (10-0)/0.1 = 100
        // out = 1*10 + 1*1 + 1*100 = 111
        float out = pid.update(10.0f, 0.1f);
        EXPECT_FLOAT_EQ(out, 111.0f);
}

TEST(PID, ResetClearsIntegralAndDerivative)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 1.0f, .kd = 1.0f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(10.0f, 0.1f);  // integral = 1.0
        EXPECT_FLOAT_EQ(pid.integral(), 1.0f);

        pid.reset();

        EXPECT_FLOAT_EQ(pid.integral(), 0.0f);
        float out = pid.update(10.0f, 0.1f);  // first call after reset: D=0
        EXPECT_FLOAT_EQ(out, 1.0f);  // P=0, I=1.0, D=0
}

TEST(PID, ResetAfterSetpointJump)
{
        PidConfig cfg{ .kp = 1.0f, .ki = 0.5f, .kd = 0.2f, .i_max = 50.0f };
        Pid pid(cfg);

        pid.update(100.0f, 0.1f);  // integral building toward 50
        pid.update(100.0f, 0.1f);
        pid.update(100.0f, 0.1f);

        pid.reset();  // new setpoint

        float out = pid.update(0.0f, 0.1f);  // error changed dramatically
        EXPECT_GE(out, -10.0f);  // no huge D spike because integral was cleared
        EXPECT_LE(out, 10.0f);
}

TEST(PID, NegativeErrorAndOutput)
{
        PidConfig cfg{ .kp = 2.0f, .ki = 1.0f, .kd = 0.5f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(0.0f, 0.1f);

        float out = pid.update(-5.0f, 0.1f);
        // P = 2 * (-5) = -10
        // I = 1 * (-5) * 0.1 = -0.5
        // D = 0.5 * (-5-0)/0.1 = -25
        // out = -10 + (-0.5) + (-25) = -35.5
        EXPECT_FLOAT_EQ(out, -35.5f);
}

TEST(PID, IntegralAccessor)
{
        PidConfig cfg{ .kp = 0.0f, .ki = 1.0f, .kd = 0.0f, .i_max = 100.0f };
        Pid pid(cfg);

        EXPECT_FLOAT_EQ(pid.integral(), 0.0f);

        pid.update(5.0f, 0.1f);  // integral += 5 * 0.1 = 0.5
        EXPECT_FLOAT_EQ(pid.integral(), 0.5f);

        pid.update(5.0f, 0.1f);  // integral = 0.5 + 5 * 0.1 = 1.0
        EXPECT_FLOAT_EQ(pid.integral(), 1.0f);
}

TEST(PID, OutputIsUnclamped)
{
        PidConfig cfg{ .kp = 10.0f, .ki = 10.0f, .kd = 0.0f, .i_max = 100.0f };
        Pid pid(cfg);

        pid.update(0.0f, 0.1f);

        float out = pid.update(100.0f, 0.1f);
        // P = 10 * 100 = 1000
        // I = 10 * (100 * 0.1) = 100
        // D = 0
        // out = 1100 (way beyond i_max which only clamps integral)
        EXPECT_GT(out, 100.0f);
        EXPECT_FLOAT_EQ(pid.integral(), 10.0f);  // integral clamped to 10
}

TEST(PID, ZeroKIStillAccumulatesIntegralDueToDtEffect)
{
        PidConfig cfg{ .kp = 1.0f, .ki = 0.0f, .kd = 0.0f, .i_max = 100.0f };
        Pid pid(cfg);

        float out1 = pid.update(10.0f, 0.1f);
        float out2 = pid.update(10.0f, 0.1f);

        EXPECT_EQ(out1, out2);  // no I term, same error gives same output
}