/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : FailureInjector.cpp / FailureInjector.hpp (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional — justification: FailureInjector reads SYS_FAILURE_EN at construction and
 *                      subscribes/publishes uORB topics (vehicle_command, vehicle_command_ack).
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (FI-D01..D13, REF_05)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/actuator_motors.h>
#include <uORB/topics/esc_status.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_command_ack.h>

#include "FailureInjector.hpp"

using namespace time_literals;

class SqeFailureInjectorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 1_s);

		// Publish a neutral command so a new subscriber never sees a previous test's injection as "updated"
		vehicle_command_s neutral{};
		neutral.timestamp = hrt_absolute_time();
		neutral.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;
		_cmd_pub.publish(neutral);

		// Drain any stale ack before acting
		vehicle_command_ack_s ack{};

		while (_ack_sub.update(&ack)) {}
	}

	static void setParamInt(const char *name, int32_t v)
	{
		ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name;
	}

	void publishInject(int unit, int type, int instance)
	{
		vehicle_command_s cmd{};
		cmd.timestamp = hrt_absolute_time();
		cmd.command = vehicle_command_s::VEHICLE_CMD_INJECT_FAILURE;
		cmd.param1 = static_cast<float>(unit);
		cmd.param2 = static_cast<float>(type);
		cmd.param3 = static_cast<float>(instance);
		_cmd_pub.publish(cmd);
	}

	static esc_status_s makeEscStatus(uint8_t esc_count)
	{
		esc_status_s esc{};
		esc.timestamp = hrt_absolute_time();
		esc.esc_count = esc_count;
		esc.esc_online_flags = 0xFF;

		for (uint8_t i = 0; i < esc_count && i < esc_status_s::CONNECTED_ESC_MAX; i++) {
			esc.esc[i].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1 + i;
			esc.esc[i].esc_voltage = 10.f + i;
			esc.esc[i].esc_current = 1.f + i;
			esc.esc[i].esc_rpm = 1000 + i;
			esc.esc[i].timestamp = esc.timestamp;
		}

		return esc;
	}

	uORB::Publication<vehicle_command_s> _cmd_pub{ORB_ID(vehicle_command)};
	uORB::Subscription _ack_sub{ORB_ID(vehicle_command_ack)};
};

// SQE-FI-01 | FI-D01 (2nd operand False — SYS_FAILURE_EN 0) FI-D02T
// Given: SYS_FAILURE_EN 0 (default); a FailureInjector constructed after the param is set
// When : inject OFF for motor 1, then update()
// Then : _failure_injection_enabled stays false (construction-time param read), so update() returns immediately
//        (FI-D02 early return); getMotorStopMask() stays 0 and no ack is published
TEST_F(SqeFailureInjectorTest, FI01_InjectionDisabled_NoEffectAndNoAck)
{
	setParamInt("SYS_FAILURE_EN", 0);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 1);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	vehicle_command_ack_s ack{};
	EXPECT_FALSE(_ack_sub.update(&ack));
}

// SQE-FI-02 | FI-D01T FI-D03 FI-D04F FI-D05 FI-D06 FI-D07(OFF) FI-D08T
// Given: SYS_FAILURE_EN 1 (read at construction); FailureInjector constructed after the param is set
// When : inject OFF for motor instance 1 (ESC index 0), then update()
// Then : getMotorStopMask() == 0x01 (bit 0 set); an ack is published with command==VEHICLE_CMD_INJECT_FAILURE and
//        result==VEHICLE_CMD_RESULT_ACCEPTED (a match was found, "supported" stays true)
TEST_F(SqeFailureInjectorTest, FI02_InjectOffInstance1_SetsStopMaskAndAcksAccepted)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 1);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0x01u);

	vehicle_command_ack_s ack{};
	ASSERT_TRUE(_ack_sub.update(&ack));
	EXPECT_EQ(ack.command, static_cast<uint32_t>(vehicle_command_s::VEHICLE_CMD_INJECT_FAILURE));
	EXPECT_EQ(ack.result, vehicle_command_ack_s::VEHICLE_CMD_RESULT_ACCEPTED);
}

// SQE-FI-03 | FI-D06 (1st operand False — instance 0 means "all ESCs")
// Given: SYS_FAILURE_EN 1
// When : inject OFF with instance 0 ("apply to all")
// Then : getMotorStopMask() == 0xFF — every ESC index 0..7 matches because FI-D06's "instance != 0" guard is
//        False, so the per-index "i != instance-1" exclusion is never evaluated (short-circuit)
TEST_F(SqeFailureInjectorTest, FI03_InjectOffInstanceZero_AppliesToAllEscs)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 0);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0xFFu);
}

// SQE-FI-04 | FI-D07(OK)
// Given: SYS_FAILURE_EN 1
// When : OFF(instance 1) then OK(instance 1); STUCK(instance 2) then OK(instance 2); each pair in its own update()
//        so the queue is processed command-by-command
// Then : after the OK commands, motor_stop_mask bit 0 is cleared and the ESC-telemetry-blocked mask bit 1 is
//        cleared; manipulateEscStatus() afterwards leaves a 2-ESC status completely unchanged (OK cleared the masks
//        FI-D10 checks)
TEST_F(SqeFailureInjectorTest, FI04_OkAfterOffAndStuck_ClearsMasksAndStopsManipulation)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 1);
	injector.update();
	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OK, 1);
	injector.update();
	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_STUCK, 2);
	injector.update();
	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OK, 2);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	esc_status_s esc = makeEscStatus(2);
	esc_status_s esc_before = esc;
	injector.manipulateEscStatus(esc);

	EXPECT_EQ(esc.esc[0].esc_voltage, esc_before.esc[0].esc_voltage);
	EXPECT_EQ(esc.esc[1].esc_voltage, esc_before.esc[1].esc_voltage);
	EXPECT_EQ(esc.esc_online_flags, esc_before.esc_online_flags);
}

// SQE-FI-05 | FI-D07(STUCK) FI-D10T FI-D11 FI-D12T
// Given: SYS_FAILURE_EN 1; STUCK injected for motor instance 2 (ESC index 1); a 4-ESC status seeded by
//        makeEscStatus() with esc_online_flags 0xFF (all bits set)
// When : manipulateEscStatus(status) is called after update() has set the blocked mask
// Then : esc[1] is zeroed except actuator_function (preserved per the production memset+restore pattern);
//        esc_online_flags has bit 1 cleared (0xFF & ~(1<<1) == 0xFD); esc[0], esc[2], esc[3] are unchanged
TEST_F(SqeFailureInjectorTest, FI05_InjectStuckMotor2_BlanksThatEscOnly)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_STUCK, 2);
	injector.update();

	esc_status_s esc = makeEscStatus(4);
	const esc_status_s esc_before = esc;
	injector.manipulateEscStatus(esc);

	EXPECT_EQ(esc.esc[1].actuator_function, actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1 + 1);
	EXPECT_EQ(esc.esc[1].esc_voltage, 0.f);
	EXPECT_EQ(esc.esc[1].esc_current, 0.f);
	EXPECT_EQ(esc.esc[1].esc_rpm, 0);
	EXPECT_EQ(esc.esc_online_flags, 0xFDu);

	EXPECT_EQ(esc.esc[0].esc_voltage, esc_before.esc[0].esc_voltage);
	EXPECT_EQ(esc.esc[2].esc_voltage, esc_before.esc[2].esc_voltage);
	EXPECT_EQ(esc.esc[3].esc_voltage, esc_before.esc[3].esc_voltage);
}

// SQE-FI-06 | FI-D07(WRONG) FI-D13T
// Given: SYS_FAILURE_EN 1; WRONG injected for motor instance 1 (ESC index 0)
// When : manipulateEscStatus(status) is called on a 1-ESC status with known voltage/current/rpm
// Then : esc[0].esc_voltage and esc_current are scaled by 0.1, esc_rpm scaled by 10 (per FailureInjector.cpp L128-130)
TEST_F(SqeFailureInjectorTest, FI06_InjectWrongMotor1_ScalesTelemetry)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_WRONG, 1);
	injector.update();

	esc_status_s esc = makeEscStatus(1);
	esc.esc[0].esc_voltage = 10.f;
	esc.esc[0].esc_current = 2.f;
	esc.esc[0].esc_rpm = 1000;
	injector.manipulateEscStatus(esc);

	EXPECT_FLOAT_EQ(esc.esc[0].esc_voltage, 1.f);
	EXPECT_FLOAT_EQ(esc.esc[0].esc_current, 0.2f);
	EXPECT_EQ(esc.esc[0].esc_rpm, 10000);
}

// SQE-FI-07 | FI-D07(no case matches) FI-D08F
// Given: SYS_FAILURE_EN 1; failure_type GARBAGE (3), which has no matching switch case
// When : update()
// Then : "supported" stays false for that ESC's switch, so the published ack result is UNSUPPORTED; the stop /
//        blocked / wrong masks are all left unchanged (still 0)
TEST_F(SqeFailureInjectorTest, FI07_InjectGarbageType_AckUnsupportedNoMaskChange)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_GARBAGE, 1);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	vehicle_command_ack_s ack{};
	ASSERT_TRUE(_ack_sub.update(&ack));
	EXPECT_EQ(ack.result, vehicle_command_ack_s::VEHICLE_CMD_RESULT_UNSUPPORTED);
}

// SQE-FI-08 | FI-D04 (1st operand True — command mismatch)
// Given: SYS_FAILURE_EN 1
// When : a DO_SET_MODE command is published (not INJECT_FAILURE) and update() is called
// Then : the command is skipped entirely (FI-D04's "command != INJECT_FAILURE" is True -> continue), so no ack is
//        published and no mask changes
TEST_F(SqeFailureInjectorTest, FI08_NonInjectCommand_IgnoredNoAck)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	vehicle_command_s cmd{};
	cmd.timestamp = hrt_absolute_time();
	cmd.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;
	_cmd_pub.publish(cmd);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	vehicle_command_ack_s ack{};
	EXPECT_FALSE(_ack_sub.update(&ack));
}

// SQE-FI-09 | FI-D04 (2nd operand True — unit mismatch)
// Given: SYS_FAILURE_EN 1
// When : an INJECT_FAILURE command with unit 0 (FAILURE_UNIT_SENSOR_GYRO, not SYSTEM_MOTOR) is published
// Then : the command is skipped (FI-D04's "unit != SYSTEM_MOTOR" is True -> continue), no ack published
TEST_F(SqeFailureInjectorTest, FI09_WrongFailureUnit_IgnoredNoAck)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SENSOR_GYRO, vehicle_command_s::FAILURE_TYPE_OFF, 1);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	vehicle_command_ack_s ack{};
	EXPECT_FALSE(_ack_sub.update(&ack));
}

// SQE-FI-10 | FI-D06 all continue, FI-D08F (via no match)
// Given: SYS_FAILURE_EN 1; OFF injected for instance 9 (1-based instance beyond CONNECTED_ESC_MAX=8, so no
//        i in [0,8) satisfies i == instance-1 == 8)
// When : update()
// Then : every iteration of the FI-D05 loop hits the FI-D06 "continue" (no index matches instance 9), so
//        "supported" stays false for the whole command -> ack UNSUPPORTED, masks unchanged
TEST_F(SqeFailureInjectorTest, FI10_InstanceBeyondRange_AckUnsupported)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 9);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);

	vehicle_command_ack_s ack{};
	ASSERT_TRUE(_ack_sub.update(&ack));
	EXPECT_EQ(ack.result, vehicle_command_ack_s::VEHICLE_CMD_RESULT_UNSUPPORTED);
}

// SQE-FI-11 | FI-D09T
// Given: SYS_FAILURE_EN 0 (disabled); no injection ever requested
// When : manipulateEscStatus() is called directly on a populated ESC status
// Then : the status is completely unchanged — FI-D09's early return fires before any mask is consulted
TEST_F(SqeFailureInjectorTest, FI11_InjectionDisabled_ManipulateIsNoOp)
{
	setParamInt("SYS_FAILURE_EN", 0);
	FailureInjector injector;

	esc_status_s esc = makeEscStatus(2);
	const esc_status_s esc_before = esc;
	injector.manipulateEscStatus(esc);

	EXPECT_EQ(esc.esc[0].esc_voltage, esc_before.esc[0].esc_voltage);
	EXPECT_EQ(esc.esc[1].esc_voltage, esc_before.esc[1].esc_voltage);
	EXPECT_EQ(esc.esc_online_flags, esc_before.esc_online_flags);
}

// SQE-FI-12 | FI-D10F
// Given: SYS_FAILURE_EN 1; no injection commands ever sent (both blocked_mask and wrong_mask are 0)
// When : manipulateEscStatus() is called directly
// Then : the status is unchanged — FI-D10's "blocked_mask != 0 || wrong_mask != 0" is False, so the per-ESC loop
//        never runs
TEST_F(SqeFailureInjectorTest, FI12_EnabledButNoFailuresInjected_ManipulateIsNoOp)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	esc_status_s esc = makeEscStatus(2);
	const esc_status_s esc_before = esc;
	injector.manipulateEscStatus(esc);

	EXPECT_EQ(esc.esc[0].esc_voltage, esc_before.esc[0].esc_voltage);
	EXPECT_EQ(esc.esc[1].esc_voltage, esc_before.esc[1].esc_voltage);
	EXPECT_EQ(esc.esc_online_flags, esc_before.esc_online_flags);
}

// SQE-FI-13 | FI-D03 multi-iteration (while loop drains a queue of commands in one update())
// Given: SYS_FAILURE_EN 1; OFF(instance 1) and STUCK(instance 2) both published before update() is called once
//        (uORB queued semantics for vehicle_command's multi-instance publication queue, ORB_QUEUE_LENGTH=8)
// When : a single update() call
// Then : both effects are applied — motor_stop_mask bit 0 set (from OFF) and the STUCK blocked mask bit 1 set,
//        observable via manipulateEscStatus() zeroing esc[1] — proving the while loop (FI-D03) processes every
//        queued command in one update() call, not just the latest
TEST_F(SqeFailureInjectorTest, FI13_TwoQueuedCommands_BothAppliedInOneUpdate)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_OFF, 1);
	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_STUCK, 2);
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0x01u);

	esc_status_s esc = makeEscStatus(2);
	injector.manipulateEscStatus(esc);
	EXPECT_EQ(esc.esc[1].esc_voltage, 0.f);
	EXPECT_EQ(esc.esc[1].esc_current, 0.f);
}

// Sanitizer-only probes (NEVER in normal builds — undefined behaviour). DISABLED by default; see P11/DECISIONS for
// the human approval required before enabling under ASan/UBSan. Not executed in this session's normal test runs.

// SQE-PRB-05 | F-07a | DISABLED — requires ASan to observe the out-of-bounds write (esc_count above CONNECTED_ESC_MAX)
// Given: SYS_FAILURE_EN 1; STUCK injected for motor instance 1 (ESC index 0)
// When : manipulateEscStatus() is called on a status whose esc_count (9) exceeds the fixed esc[8] array bound and
//        FI-D11's loop (`i < status.esc_count`) has no clamp to CONNECTED_ESC_MAX unlike the sibling loop at L67
// Then : expected behaviour under ASan is a detected heap/stack-buffer-overflow on the out-of-bounds esc[8] write
TEST_F(SqeFailureInjectorTest, DISABLED_PRB05_EscCountAboveMax)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_STUCK, 1);
	injector.update();

	esc_status_s esc = makeEscStatus(8);
	esc.esc_count = 9; // out-of-bounds vs. the fixed esc[8] array (F-07a)
	injector.manipulateEscStatus(esc);

	SUCCEED() << "Not run in normal builds; requires ASan to observe the overflow (F-07a).";
}

// SQE-PRB-06 | F-07b | DISABLED — requires UBSan to observe the unguarded shift (non-motor actuator_function)
// Given: SYS_FAILURE_EN 1; STUCK injected (sets a nonzero blocked_mask)
// When : manipulateEscStatus() is called on a status whose esc[0].actuator_function is 0 (not a motor function),
//        so FI-D12's `i_esc = actuator_function - ACTUATOR_FUNCTION_MOTOR1` underflows to a huge unsigned value
//        and `1 << i_esc` becomes an out-of-range shift exponent (no guard, unlike FD-D31 in FailureDetector)
// Then : expected behaviour under UBSan is a detected "shift exponent too large" diagnostic (F-07b)
TEST_F(SqeFailureInjectorTest, DISABLED_PRB06_NonMotorFunctionShift)
{
	setParamInt("SYS_FAILURE_EN", 1);
	FailureInjector injector;

	publishInject(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR, vehicle_command_s::FAILURE_TYPE_STUCK, 1);
	injector.update();

	esc_status_s esc = makeEscStatus(1);
	esc.esc[0].actuator_function = 0; // not ACTUATOR_FUNCTION_MOTOR1-based (F-07b)
	injector.manipulateEscStatus(esc);

	SUCCEED() << "Not run in normal builds; requires UBSan to observe the unguarded shift (F-07b).";
}
