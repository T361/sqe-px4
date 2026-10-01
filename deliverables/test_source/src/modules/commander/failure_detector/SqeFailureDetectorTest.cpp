/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : FailureDetector.cpp / FailureDetector.hpp (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional — justification: FailureDetector reads params at construction and
 *                      subscribes to uORB topics (vehicle_attitude, pwm_input, esc_status, actuator_motors,
 *                      sensor_selection, vehicle_imu_status); vehicle_status/vehicle_control_mode are passed
 *                      directly as update() arguments.
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (FD-D01..D45, REF_05)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/topics/actuator_motors.h>
#include <uORB/topics/esc_status.h>
#include <uORB/topics/pwm_input.h>
#include <uORB/topics/vehicle_attitude.h>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/vehicle_status.h>

#include "FailureDetector.hpp"

using namespace time_literals;

namespace
{
// q = [w,x,y,z] for a pure roll rotation of `roll_deg` degrees (pitch=yaw=0)
void setRollQuaternion(float q[4], float roll_deg)
{
	const float half = math::radians(roll_deg) / 2.f;
	q[0] = cosf(half);
	q[1] = sinf(half);
	q[2] = 0.f;
	q[3] = 0.f;
}

// q = [w,x,y,z] for a pure pitch rotation of `pitch_deg` degrees (roll=yaw=0)
void setPitchQuaternion(float q[4], float pitch_deg)
{
	const float half = math::radians(pitch_deg) / 2.f;
	q[0] = cosf(half);
	q[1] = 0.f;
	q[2] = sinf(half);
	q[3] = 0.f;
}
}

class SqeFailureDetectorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 1_s);

		_status = {};
		_status.arming_state = vehicle_status_s::ARMING_STATE_ARMED;
		_status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;

		_mode = {};
		_mode.flag_control_attitude_enabled = true;

		// Default isolation: disable the subsystems not under test in a given test (overridden per test as needed)
		setParamInt("FD_EXT_ATS_EN", 0);
		setParamInt("FD_IMB_PROP_THR", 0);
		setParamInt("SYS_FAILURE_EN", 0);

		// vehicle_command is a queued uORB topic (ORB_QUEUE_LENGTH=8) that is never drained if a previous test's
		// FailureDetector/FailureInjector was destroyed before consuming everything it published. A fresh
		// subscription inside the next FailureDetector's FailureInjector would otherwise see that leftover command
		// as "new" on its very first update() (uORB latest-sample semantics for the newest generation available).
		// Publishing a neutral, non-INJECT_FAILURE command here guarantees every test starts from a clean slate,
		// independent of execution order (R11).
		vehicle_command_s neutral{};
		neutral.timestamp = hrt_absolute_time();
		neutral.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;
		_cmd_pub.publish(neutral);
	}

	static void setParamInt(const char *name, int32_t v)
	{
		ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name;
	}
	static void setParamFloat(const char *name, float v)
	{
		ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name;
	}

	void publishAttitude(float roll_deg, float pitch_deg)
	{
		vehicle_attitude_s att{};
		att.timestamp = hrt_absolute_time();
		// combine roll then pitch (small-angle tests use one axis at a time so simple composition is fine)
		float q_roll[4];
		setRollQuaternion(q_roll, roll_deg);
		float q_pitch[4];
		setPitchQuaternion(q_pitch, pitch_deg);
		const matrix::Quatf q = matrix::Quatf(q_roll) * matrix::Quatf(q_pitch);
		q.copyTo(att.q);
		_att_pub.publish(att);
	}

	void publishEscStatus(uint8_t esc_count, uint8_t armed_flags, uint8_t failures0 = 0)
	{
		esc_status_s esc{};
		esc.timestamp = hrt_absolute_time();
		esc.esc_count = esc_count;
		esc.esc_armed_flags = armed_flags;

		for (uint8_t i = 0; i < esc_count && i < esc_status_s::CONNECTED_ESC_MAX; i++) {
			esc.esc[i].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1 + i;
			esc.esc[i].timestamp = esc.timestamp;
		}

		if (esc_count > 0) {
			esc.esc[0].failures = failures0;
		}

		_esc_pub.publish(esc);
	}

	uORB::Publication<vehicle_attitude_s> _att_pub{ORB_ID(vehicle_attitude)};
	uORB::Publication<pwm_input_s> _pwm_pub{ORB_ID(pwm_input)};
	uORB::Publication<esc_status_s> _esc_pub{ORB_ID(esc_status)};
	uORB::Publication<actuator_motors_s> _act_pub{ORB_ID(actuator_motors)};
	uORB::Publication<vehicle_command_s> _cmd_pub{ORB_ID(vehicle_command)};

	vehicle_status_s _status{};
	vehicle_control_mode_s _mode{};
};

// SQE-FD-01 | FD-D01 (T then F)
// Given: attitude control enabled, roll 70° (> default FD_FAIL_R 60°), long enough hysteresis time (default 0.3s)
// When : update() repeatedly for > 1.5x the 0.3s roll hysteresis, then flag_control_attitude_enabled=false, update()
// Then : roll flag latches true once the hysteresis settles; once attitude control is disabled, roll/pitch/alt/ext
//        are all reset false and update() reports a status change (return true)
TEST_F(SqeFailureDetectorTest, FD01_AttitudeControlDisabled_ResetsAllAttitudeFlags)
{
	FailureDetector fd{nullptr};

	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(450)); // 1.5x 300ms default hysteresis
	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);

	ASSERT_TRUE(fd.getStatusFlags().roll);

	_mode.flag_control_attitude_enabled = false;
	const bool changed = fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().roll);
	EXPECT_FALSE(fd.getStatusFlags().pitch);
	EXPECT_FALSE(fd.getStatusFlags().alt);
	EXPECT_FALSE(fd.getStatusFlags().ext);
	EXPECT_TRUE(changed);
}

// SQE-FD-02 | FD-D08T FD-D12 (T∧T)
// Given: FD_FAIL_R 60 (default), FD_FAIL_R_TTRI 0.02s (documented minimum); roll 70°
// When : first update() (starts hysteresis); wait 40ms (2x the 0.02s trigger time); publish again; update()
// Then : roll flag false right after the first update (hysteresis not yet elapsed), true after the second
TEST_F(SqeFailureDetectorTest, FD02_RollBeyondLimit_FlagSetsAfterHysteresis)
{
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	EXPECT_FALSE(fd.getStatusFlags().roll);

	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(70.f, 0.f);
	const bool changed = fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().roll);
	EXPECT_TRUE(changed);
}

// SQE-FD-03 | FD-D12 (T∧F)
// Given: FD_FAIL_R 60 (default), FD_FAIL_R_TTRI 0.02s; roll 50° (below the 60° limit)
// When : update(); wait 40ms; publish again; update()
// Then : roll flag stays false (fabsf(roll) > max_roll is False)
TEST_F(SqeFailureDetectorTest, FD03_RollWithinLimit_FlagStaysFalse)
{
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(50.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(50.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().roll);

	// Positive control: same detector and timing, roll beyond the limit -> flag sets (the FALSE above is value-driven)
	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().roll);
}

// SQE-FD-04 | FD-D12 (1st operand False — check disabled)
// Given: FD_FAIL_R 0 (disables the roll check: max_roll > FLT_EPSILON is False); roll 170°
// When : update() repeatedly past the default hysteresis time
// Then : roll flag stays false regardless of the extreme roll angle, because the check is disabled at the source
TEST_F(SqeFailureDetectorTest, FD04_RollLimitZero_ChecksDisabled)
{
	setParamInt("FD_FAIL_R", 0);
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(170.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(170.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().roll);

	// Positive control: identical stimulus with the check enabled (FD_FAIL_R 60, read at construction) -> flag sets
	setParamInt("FD_FAIL_R", 60);
	FailureDetector fd_enabled{nullptr};
	publishAttitude(170.f, 0.f);
	fd_enabled.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(170.f, 0.f);
	fd_enabled.update(_status, _mode);
	EXPECT_TRUE(fd_enabled.getStatusFlags().roll);
}

// SQE-FD-05 | FD-D13 (pitch analogue of FD02)
// Given: FD_FAIL_P 60 (default), FD_FAIL_P_TTRI 0.02s; pitch 70°
// When : update(); wait 40ms; publish again; update()
// Then : pitch flag true after the hysteresis settles
TEST_F(SqeFailureDetectorTest, FD05_PitchBeyondLimit_FlagSetsAfterHysteresis)
{
	setParamFloat("FD_FAIL_P_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(0.f, 70.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, 70.f);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().pitch);
}

// SQE-FD-06 | FD-D13 (pitch analogue of FD03)
// Given: FD_FAIL_P 60 (default), FD_FAIL_P_TTRI 0.02s; pitch 50°
// When : update(); wait 40ms; publish again; update()
// Then : pitch flag stays false (within limit)
TEST_F(SqeFailureDetectorTest, FD06_PitchWithinLimit_FlagStaysFalse)
{
	setParamFloat("FD_FAIL_P_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(0.f, 50.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, 50.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().pitch);

	// Positive control: same detector and timing, pitch beyond the limit -> flag sets
	publishAttitude(0.f, 70.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, 70.f);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().pitch);
}

// SQE-FD-07 | FD-D13 (pitch analogue of FD04)
// Given: FD_FAIL_P 0 (disables the pitch check); pitch 80° (beyond the 60° default limit). 80° rather than 170°:
//        Euler pitch only spans ±90°, so a 170° pitch rotation decomposes to pitch 10° / roll 180° and would never
//        trip the pitch check even when enabled (found by this test's positive control)
// When : update() repeatedly past the default hysteresis time
// Then : pitch flag stays false regardless of the extreme pitch, because the check is disabled at the source
TEST_F(SqeFailureDetectorTest, FD07_PitchLimitZero_ChecksDisabled)
{
	setParamInt("FD_FAIL_P", 0);
	setParamFloat("FD_FAIL_P_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(0.f, 80.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, 80.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().pitch);

	// Positive control: identical stimulus with the check enabled (FD_FAIL_P 60, read at construction) -> flag sets
	setParamInt("FD_FAIL_P", 60);
	FailureDetector fd_enabled{nullptr};
	publishAttitude(0.f, 80.f);
	fd_enabled.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, 80.f);
	fd_enabled.update(_status, _mode);
	EXPECT_TRUE(fd_enabled.getStatusFlags().pitch);
}

// SQE-FD-08 | FD-D08F (second update without a new attitude publish)
// Given: FD_FAIL_R_TTRI 0.02s; roll 70° published once, update() called to seed state
// When : a second update() is called without publishing a new attitude message
// Then : flags are unchanged (FD-D08's _vehicle_attitude_sub.update() returns false, body skipped) and the return
//        value of update() is false (no status change)
TEST_F(SqeFailureDetectorTest, FD08_NoNewAttitude_FlagsUnchangedAndNoChangeReported)
{
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	ASSERT_TRUE(fd.getStatusFlags().roll);

	const bool changed = fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().roll);
	EXPECT_FALSE(changed);
}

// SQE-FD-09 | FD-D09T FD-D10T
// Given: tailsitter (is_vtol_tailsitter=true), in_transition_mode=true; roll 80° (beyond default limit)
// When : update() repeatedly past the default hysteresis time
// Then : during tailsitter transition the attitude check is disabled (roll/pitch forced to 0 before the limit
//        comparison), so the roll flag stays false despite the large published roll
TEST_F(SqeFailureDetectorTest, FD09_TailsitterInTransition_AttitudeCheckDisabled)
{
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	_status.is_vtol_tailsitter = true;
	_status.in_transition_mode = true;
	FailureDetector fd{nullptr};

	publishAttitude(80.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(80.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().roll);

	// Positive control: same tailsitter (rotary wing) once the transition ends -> the 80 degree roll is detected
	_status.in_transition_mode = false;
	publishAttitude(80.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(80.f, 0.f);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().roll);
}

// SQE-FD-10 | FD-D11T (FW: rotate +90° pitch)
// Given: tailsitter, not in transition; a single attitude of pitch -80° published twice (hysteresis settled)
// When : vehicle_type = FIXED_WING
// Then : the attitude is rotated +90° about pitch before the limit check, turning pitch -80 into roll domain
//        (euler_rotated.theta() ~ -(90-80)=... ) leaving the *pitch* flag false. Paired with SQE-FD-34 (FD-D11F,
//        same input, ROTARY_WING) to cover both branches of the same decision.
TEST_F(SqeFailureDetectorTest, FD10_TailsitterFixedWing_RotatesAttitudeBeforeCheck)
{
	setParamFloat("FD_FAIL_P_TTRI", 0.02f);
	_status.is_vtol_tailsitter = true;
	_status.in_transition_mode = false;
	_status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_FIXED_WING;
	FailureDetector fd{nullptr};

	publishAttitude(0.f, -80.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, -80.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().pitch);
}

// SQE-FD-34 | FD-D11F (MC: no rotation) — paired with SQE-FD-10 (same decision, True branch)
// Given: tailsitter, not in transition; a single attitude of pitch -80° published twice (hysteresis settled)
// When : vehicle_type = ROTARY_WING
// Then : no rotation is applied so the raw -80° pitch exceeds the 60° limit and the pitch flag is true
TEST_F(SqeFailureDetectorTest, FD34_TailsitterRotaryWing_NoRotationAppliedToCheck)
{
	setParamFloat("FD_FAIL_P_TTRI", 0.02f);
	_status.is_vtol_tailsitter = true;
	_status.in_transition_mode = false;
	_status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;
	FailureDetector fd{nullptr};

	publishAttitude(0.f, -80.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(0.f, -80.f);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().pitch);
}

// SQE-FD-11 | FD-D02T FD-D14T FD-D15 (T∧T)
// Given: FD_EXT_ATS_EN 1, FD_EXT_ATS_TRIG 1900; pulse 2000us (inside [1900,3000) window), ATS hysteresis fixed 100ms
// When : update(); wait 150ms (1.5x 100ms); publish pwm_input again; update()
// Then : ext flag becomes true once the hysteresis settles
TEST_F(SqeFailureDetectorTest, FD11_ExternalAtsPulseInWindow_FlagSetsAfterHysteresis)
{
	setParamInt("FD_EXT_ATS_EN", 1);
	FailureDetector fd{nullptr};

	pwm_input_s pwm{};
	pwm.timestamp = hrt_absolute_time();
	pwm.pulse_width = 2000;
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	pwm.timestamp = hrt_absolute_time();
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().ext);
}

// SQE-FD-12 | FD-D15 boundaries [trig, 3000us)
// Given: FD_EXT_ATS_EN 1, FD_EXT_ATS_TRIG 1900 (default); four independent detectors, one pulse width each
// When : pulse 1899 / 1900 / 2999 / 3000, each settled past the 100ms ATS hysteresis
// Then : 1899 -> false (below trig); 1900 -> true (>= trig); 2999 -> true (< 3000); 3000 -> false (not < 3000)
TEST_F(SqeFailureDetectorTest, FD12_PulseWidthBoundaries_WindowIsHalfOpen)
{
	setParamInt("FD_EXT_ATS_EN", 1);

	auto runWithPulse = [this](uint32_t pulse_width) {
		FailureDetector fd{nullptr};
		pwm_input_s pwm{};
		pwm.timestamp = hrt_absolute_time();
		pwm.pulse_width = pulse_width;
		_pwm_pub.publish(pwm);
		fd.update(_status, _mode);
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		pwm.timestamp = hrt_absolute_time();
		_pwm_pub.publish(pwm);
		fd.update(_status, _mode);
		return fd.getStatusFlags().ext;
	};

	EXPECT_FALSE(runWithPulse(1899));
	EXPECT_TRUE(runWithPulse(1900));
	EXPECT_TRUE(runWithPulse(2999));
	EXPECT_FALSE(runWithPulse(3000));
}

// SQE-FD-13 | FD-D02F
// Given: FD_EXT_ATS_EN 0 (default); pulse 2000 (would trigger if enabled)
// When : update() repeatedly past the ATS hysteresis time
// Then : ext flag stays false because updateExternalAtsStatus() is never called (FD-D02 False)
TEST_F(SqeFailureDetectorTest, FD13_ExternalAtsDisabled_ExtFlagNeverSet)
{
	FailureDetector fd{nullptr};

	pwm_input_s pwm{};
	pwm.timestamp = hrt_absolute_time();
	pwm.pulse_width = 2000;
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	pwm.timestamp = hrt_absolute_time();
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().ext);

	// Positive control: identical pulse with FD_EXT_ATS_EN 1 (read at construction) -> ext flag sets
	setParamInt("FD_EXT_ATS_EN", 1);
	FailureDetector fd_enabled{nullptr};
	pwm.timestamp = hrt_absolute_time();
	_pwm_pub.publish(pwm);
	fd_enabled.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	pwm.timestamp = hrt_absolute_time();
	_pwm_pub.publish(pwm);
	fd_enabled.update(_status, _mode);
	EXPECT_TRUE(fd_enabled.getStatusFlags().ext);
}

// SQE-FD-14 | FD-D14F
// Given: FD_EXT_ATS_EN 1; pwm published once and consumed by a first update() that settles the ext flag true
// When : a second update() is called without a new pwm_input publish
// Then : ext flag is unchanged from before (still true) — _pwm_input_sub.update() returns false so the hysteresis
//        is never re-evaluated
TEST_F(SqeFailureDetectorTest, FD14_NoNewPwm_ExtFlagUnchanged)
{
	setParamInt("FD_EXT_ATS_EN", 1);
	FailureDetector fd{nullptr};

	pwm_input_s pwm{};
	pwm.timestamp = hrt_absolute_time();
	pwm.pulse_width = 2000;
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	pwm.timestamp = hrt_absolute_time();
	_pwm_pub.publish(pwm);
	fd.update(_status, _mode);
	ASSERT_TRUE(fd.getStatusFlags().ext);

	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().ext);
}

// SQE-FD-15 | FD-D03T FD-D04T FD-D16T FD-D17 FD-D18F FD-D19F
// Given: FD_ESCS_EN 1 (default); armed; 4 ESCs, all armed (flags 0x0F), no individual failures
// When : update(); wait 350ms (1.5x the 300ms ESC hysteresis); publish esc_status again; update()
// Then : arm_escs stays false — all ESCs armed and healthy means is_esc_failure is false throughout
TEST_F(SqeFailureDetectorTest, FD15_AllEscsArmedNoFailures_ArmEscsStaysFalse)
{
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x0F);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x0F);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().arm_escs);

	// Positive control: same detector, one ESC drops out of the armed mask (0x07) -> arm_escs sets after hysteresis
	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().arm_escs);
}

// SQE-FD-16 | FD-D19T (300ms hysteresis)
// Given: FD_ESCS_EN 1; armed; 4 ESCs expected but only 3 armed (flags 0x07, all_escs_armed_mask 0x0F mismatch)
// When : update(); wait 350ms (1.5x 300ms); publish again; update()
// Then : arm_escs becomes true once the hysteresis settles on the persistent mismatch
TEST_F(SqeFailureDetectorTest, FD16_EscArmedMismatch_ArmEscsSetsAfterHysteresis)
{
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x07);
	const bool changed = fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().arm_escs);
	EXPECT_TRUE(changed);
}

// SQE-FD-17 | FD-D18 (2nd operand True: per-ESC failures nonzero)
// Given: FD_ESCS_EN 1; armed; 4 ESCs, all armed (flags 0x0F, mask matches) but esc[0].failures = 1
// When : update(); wait 350ms; publish again; update()
// Then : arm_escs becomes true via the per-ESC failures OR-term even though the armed mask matches exactly
TEST_F(SqeFailureDetectorTest, FD17_PerEscFailureFlag_ArmEscsSetsEvenWithMatchingMask)
{
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x0F, 1);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x0F, 1);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().arm_escs);
}

// SQE-FD-18 | FD-D19 (no else branch — latched while armed)
// Given: after FD16-style trigger (arm_escs true), ESCs then report healthy (flags 0x0F, no failures) while armed
// When : update() with the healthy report; wait 350ms; publish again; update()
// Then : arm_escs stays true — FD-D19 only ever sets the flag to true (get_state()), there is no branch that resets
//        it to false while armed; only the disarmed else-branch resets it (see FD19)
TEST_F(SqeFailureDetectorTest, FD18_HealthyAfterTrigger_ArmEscsStaysLatchedWhileArmed)
{
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x07); // trigger
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	ASSERT_TRUE(fd.getStatusFlags().arm_escs);

	publishEscStatus(4, 0x0F); // now healthy
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x0F);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().arm_escs);
}

// SQE-FD-19 | FD-D16F
// Given: FD_ESCS_EN 1; vehicle disarmed; ESC mismatch that would trigger arm_escs if armed
// When : update()
// Then : arm_escs is false immediately — the disarmed else-branch resets the hysteresis and the flag synchronously
TEST_F(SqeFailureDetectorTest, FD19_Disarmed_ArmEscsStaysFalse)
{
	_status.arming_state = vehicle_status_s::ARMING_STATE_DISARMED;
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().arm_escs);

	// Positive control: same detector and mismatch once armed -> arm_escs sets after the 300ms hysteresis
	_status.arming_state = vehicle_status_s::ARMING_STATE_ARMED;
	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x07);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().arm_escs);
}

// SQE-FD-20 | FD-D17 (esc_count clamp to CONNECTED_ESC_MAX=8)
// Given: FD_ESCS_EN 1; armed; esc_count 12 (above CONNECTED_ESC_MAX), all 8 clamp-relevant bits in esc_armed_flags
//        set (0xFF, matching the clamped 8-ESC mask), no per-ESC failures within the clamped range
// When : update(); wait 350ms; publish again; update()
// Then : arm_escs stays false — limited_esc_count is clamped to 8, so the extra (unclamped) ESCs beyond index 8
//        are never iterated and the armed mask (0xFF for 8 ESCs) matches exactly
TEST_F(SqeFailureDetectorTest, FD20_EscCountAboveMax_ClampedToEight)
{
	FailureDetector fd{nullptr};

	publishEscStatus(8, 0xFF); // esc_count field set to 12 below, but only 8 esc[] slots exist/clamp to 8
	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 12;
	esc.esc_armed_flags = 0xFF;

	for (uint8_t i = 0; i < esc_status_s::CONNECTED_ESC_MAX; i++) {
		esc.esc[i].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1 + i;
		esc.esc[i].timestamp = esc.timestamp;
	}

	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	esc.timestamp = hrt_absolute_time();
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().arm_escs);

	// Positive control: same esc_count 12, but ESC 7 missing from the armed flags (0x7F) -> the clamped 8-ESC mask
	// (0xFF) no longer matches, so arm_escs sets; shows the clamp still checks all 8 slots
	FailureDetector fd_mismatch{nullptr};
	esc.esc_armed_flags = 0x7F;
	esc.timestamp = hrt_absolute_time();
	_esc_pub.publish(esc);
	fd_mismatch.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	esc.timestamp = hrt_absolute_time();
	_esc_pub.publish(esc);
	fd_mismatch.update(_status, _mode);
	EXPECT_TRUE(fd_mismatch.getStatusFlags().arm_escs);
}

// SQE-FD-21 | FD-D04F
// Given: FD_ESCS_EN 0; armed; ESC status reporting 0 armed ESCs (flags 0x00, which would trigger if the check ran)
// When : update(); wait 350ms; publish again; update()
// Then : arm_escs stays false because updateEscsStatus() is never called (FD-D04 False)
TEST_F(SqeFailureDetectorTest, FD21_EscsCheckDisabled_ArmEscsNeverSet)
{
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x00);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x00);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().arm_escs);

	// Positive control: identical stimulus with FD_ESCS_EN 1 (read at construction) -> arm_escs sets
	setParamInt("FD_ESCS_EN", 1);
	FailureDetector fd_enabled{nullptr};
	publishEscStatus(4, 0x00);
	fd_enabled.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(350));
	publishEscStatus(4, 0x00);
	fd_enabled.update(_status, _mode);
	EXPECT_TRUE(fd_enabled.getStatusFlags().arm_escs);
}

// SQE-FD-22 | FD-D03F
// Given: FD_ESCS_EN 1, FD_ACT_EN 1; esc_status published once and consumed by an initial update()
// When : a second update() is called without a new esc_status publish
// Then : both ESC/motor subroutines are skipped (FD-D03 False — the shared esc_status subscriber sees no update),
//        so getMotorFailures() stays 0 and arm_escs is unchanged from before
TEST_F(SqeFailureDetectorTest, FD22_NoNewEscStatus_EscAndMotorLogicSkipped)
{
	setParamFloat("FD_ACT_MOT_TOUT", 10.f);
	FailureDetector fd{nullptr};

	publishEscStatus(4, 0x0F);
	fd.update(_status, _mode);
	ASSERT_EQ(fd.getMotorFailures(), 0u);

	const bool changed = fd.update(_status, _mode);

	EXPECT_EQ(fd.getMotorFailures(), 0u);
	EXPECT_FALSE(fd.getStatusFlags().arm_escs);
	EXPECT_FALSE(changed);
}

// SQE-FD-23 | FD-D32 FD-D33T/F FD-D34T FD-D42 FD-D43T FD-D44T
// Given: FD_ACT_EN 1 (default), FD_ESCS_EN 0 (isolate); armed; ESC0 reports current 5A fresh (warms up the
//        has_current latch and marks valid_current_mask)
// When : (1) esc[0].timestamp set to now-400ms (> 300ms timeout) with current still >0, update() -> times out;
//        (2) immediately another stale update with the same old timestamp (esc_timeout_currently_flagged already
//        true, FD-D33's 3rd operand is False so the flag is not re-set, but it is also not cleared) -> stays set;
//        (3) a fresh (non-timed-out) esc_status update -> FD-D34 clears the timed-out bit
// Then : (1) motor flag true and getMotorFailures()&1 == 1; (2) motor flag still true (latched, not re-triggered);
//        (3) motor flag false once the timeout clears and under-current was never latched
TEST_F(SqeFailureDetectorTest, FD23_EscTelemetryTimeout_SetsThenClearsMotorFlag)
{
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	// Warm-up: fresh current reading, valid timestamp
	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_EQ(fd.getMotorFailures(), 0u);

	// Step 1: stale timestamp (400ms old, > 300ms timeout), still "valid" from warm-up
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures() & 0x01, 0x01u);

	// Step 2: publish again with the same stale timestamp -> flag stays set (not re-triggered, not cleared)
	esc.timestamp = hrt_absolute_time();
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures() & 0x01, 0x01u);

	// Step 3: fresh timestamp -> timed-out bit clears (FD-D34), under-current never latched -> motor flag clears
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-24 | FD-D32F FD-D33F FD-D35F FD-D36F
// Given: FD_ACT_EN 1; armed; ESC0 never reports current (esc_current == 0 always) with an old timestamp
// When : update()
// Then : no timeout flag is raised — the ESC was never "valid" (FD-D32's second operand, current > 0, is always
//        False so valid_current_mask bit never sets), so esc_was_valid is False and FD-D33 cannot fire; FD-D35 is
//        also False (current never > FLT_EPSILON) so the has_current latch (FD-D36) never engages either
TEST_F(SqeFailureDetectorTest, FD24_EscNeverReportsCurrent_NoTimeoutFlag)
{
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms; // old from the start
	esc.esc[0].esc_current = 0.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-25 | FD-D36T FD-D37T FD-D38T FD-D39T FD-D41T
// Given: FD_ACT_EN 1, FD_ACT_MOT_THR 0.2, FD_ACT_MOT_C2T 2.0, FD_ACT_MOT_TOUT 10ms; armed; ESC0 warmed up with
//        current 5A (latches has_current); then actuator control 0.5 (above 0.2 threshold) with esc_current 0.1A
//        (below 0.5*2.0=1.0 threshold -> current_too_low True); esc timestamp kept fresh (not timed out)
// When : update() starts the under-current timer; wait 30ms (1.5x the 10ms timeout, > 10ms so trigger fires);
//        publish esc_status again (fresh timestamp, same low-current/high-throttle condition); update()
// Then : motor flag becomes true and stays set (FD-D41's mask bit is latched, "never cleared" per source comment)
TEST_F(SqeFailureDetectorTest, FD25_UnderCurrentWhileArmed_LatchesMotorFlagAfterTimeout)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 10);
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f;
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f; // warm up has_current
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_FALSE(fd.getStatusFlags().motor);

	// Low current, throttle above threshold -> starts the under-current timer
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 0.1f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	const bool changed = fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().motor);
	EXPECT_TRUE(changed);
}

// SQE-FD-26 | FD-D38F FD-D40T
// Given: same isolation/params as FD25 but FD_ACT_MOT_TOUT 1000ms; ESC0 warmed up; under-current timer started at
//        t_start (low current, throttle above threshold)
// When : after 600ms, one sample with throttle below the threshold (control 0.1 < 0.2) -> throttle_above_threshold
//        False; then throttle back above threshold with low current -> timer restarts at t_restart; after another
//        600ms, update() at t_check
// Then : at t_check the flag is still false although t_check - t_start > 1000ms — which proves the original timer was
//        reset (FD-D40); without the reset it would have fired. Then, once t - t_restart > 1000ms, the flag becomes
//        true (positive control: the restarted timer is live).
// Determinism: the board has no lockstep clock (CONFIG_BOARD_NOLOCKSTEP), so the test measures its own timestamps.
//        The lower bounds (> 1000ms) always hold because sleep_for never returns early. The one upper bound
//        (t_check - t_restart < 1000ms) has 400ms of slack and is asserted as an explicit precondition, so a scheduler
//        stall is reported as an environment problem, never as a wrong product result.
TEST_F(SqeFailureDetectorTest, FD26_ThrottleDropsBeforeTimeout_ResetsUnderCurrentTimer)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 1000);
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f;
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;

	auto publishEscCurrent = [&](float current) {
		esc.timestamp = hrt_absolute_time();
		esc.esc[0].timestamp = esc.timestamp;
		esc.esc[0].esc_current = current;
		_esc_pub.publish(esc);
	};
	auto publishThrottle = [&](float control) {
		act.timestamp = hrt_absolute_time();
		act.control[0] = control;
		_act_pub.publish(act);
	};

	publishEscCurrent(5.f); // warm up has_current
	fd.update(_status, _mode);

	publishEscCurrent(0.1f); // starts the timer
	fd.update(_status, _mode);
	const hrt_abstime t_start = hrt_absolute_time(); // taken after the update: the real start is no later than this
	ASSERT_FALSE(fd.getStatusFlags().motor);

	std::this_thread::sleep_for(std::chrono::milliseconds(600));

	// Throttle below threshold -> condition false -> timer reset (FD-D40)
	publishThrottle(0.1f);
	publishEscCurrent(0.1f);
	fd.update(_status, _mode);
	ASSERT_FALSE(fd.getStatusFlags().motor);

	// Throttle above threshold again with low current -> timer restarts
	publishThrottle(0.5f);
	publishEscCurrent(0.1f);
	const hrt_abstime t_restart = hrt_absolute_time(); // taken before the update: the real restart is no earlier
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(600));

	publishEscCurrent(0.1f);
	const hrt_abstime t_check_before = hrt_absolute_time();
	fd.update(_status, _mode);
	const hrt_abstime t_check_after = hrt_absolute_time();

	// Bracketing the update with two timestamps makes both bounds conservative for the `now` used inside update()
	ASSERT_GT(t_check_before - t_start, 1000_ms) << "precondition: original timer would have expired without a reset";
	ASSERT_LT(t_check_after - t_restart, 1000_ms) << "environment: scheduler stall exceeded the 400ms timing slack";
	EXPECT_FALSE(fd.getStatusFlags().motor);

	// Positive control: past 1000ms after the restart the timer fires
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	publishEscCurrent(0.1f);
	fd.update(_status, _mode);
	EXPECT_TRUE(fd.getStatusFlags().motor);
}

// SQE-FD-27 | FD-D37F (NaN control treated as zero throttle)
// Given: same isolation/params as FD25; ESC0 warmed up; actuator control[0] = NaN, esc_current low
// When : update()
// Then : FD-D37 is False (PX4_ISFINITE(NaN) is false), so esc_throttle is forced to 0; throttle_above_threshold
//        (0 > 0.2) is False, so no under-current timer starts and the motor flag stays false
TEST_F(SqeFailureDetectorTest, FD27_NanControl_TreatedAsZeroThrottle)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 10);
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = std::numeric_limits<float>::quiet_NaN();
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f; // warm up has_current
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 0.1f;
	_esc_pub.publish(esc);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-28 | FD-D31T (unsigned wrap guard for non-motor actuator functions)
// Given: FD_ACT_EN 1; armed; esc[0].actuator_function = 0 (not a motor function; i_esc wraps to a huge unsigned
//        value under actuator_function - ACTUATOR_FUNCTION_MOTOR1); old timestamp, nonzero current
// When : update()
// Then : the ESC is skipped entirely (FD-D31's guard continues the loop before any mask uses i_esc), so no motor
//        flag is raised and getMotorFailures() stays 0 despite the stale timestamp and nonzero current
TEST_F(SqeFailureDetectorTest, FD28_NonMotorActuatorFunction_SkippedByUnsignedGuard)
{
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = 0; // not ACTUATOR_FUNCTION_MOTOR1-based
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-29 | FD-D29F FD-D45
// Given: following the FD25 under-current latch sequence (motor flag true, under_current_mask bit 0 set)
// When : vehicle_status.arming_state is set to DISARMED and esc_status is republished to trigger the update
// Then : motor flag becomes false and getMotorFailures() becomes 0 — the disarmed branch unconditionally resets
//        the undercurrent timers, the under_current_mask, and the motor flag (FD-D29 False path, FD-D45 loop)
TEST_F(SqeFailureDetectorTest, FD29_DisarmAfterUnderCurrentLatch_ClearsMotorFlagAndMask)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 10);
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f;
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 0.1f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_TRUE(fd.getStatusFlags().motor);
	ASSERT_EQ(fd.getMotorFailures() & 0x01, 0x01u);

	_status.arming_state = vehicle_status_s::ARMING_STATE_DISARMED;
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-30 | FD-D05F
// Given: FD_ACT_EN 0; armed; ESC0 with an old (timed-out) timestamp and nonzero current, which would raise the
//        motor flag if the motor-status subroutine ran
// When : update()
// Then : motor flag stays false and getMotorFailures() stays 0 because updateMotorStatus() is never called
//        (FD-D05 False)
TEST_F(SqeFailureDetectorTest, FD30_MotorCheckDisabled_MotorLogicSkipped)
{
	setParamInt("FD_ACT_EN", 0);
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures(), 0u);
}

// SQE-FD-31 | integration of FI-D10/D12 with FD-D33 (FailureInjector in FailureDetector's control path)
// Given: SYS_FAILURE_EN 1 (enables injection, read at FailureInjector() construction, which happens inside the
//        FailureDetector constructor); armed; ESC0 valid with fresh current, warmed up
// When : a vehicle_command INJECT_FAILURE (SYSTEM_MOTOR, STUCK, instance 1) is published and consumed by
//        fd.update()'s internal _failure_injector.update() call; the next esc_status publish is then blanked out
//        by manipulateEscStatus() before updateMotorStatus() runs on it (same update() call chain, FD.cpp L73-82)
// Then : the ESC's telemetry timestamp is zeroed by the injector (memset), which updateMotorStatus() reads as an
//        old/zero timestamp -> esc_timed_out becomes true -> the motor timeout flag is raised, proving the
//        injector's STUCK failure is wired into the detector's own ESC-timeout branch
TEST_F(SqeFailureDetectorTest, FD31_InjectedStuckFailure_TriggersDetectorMotorTimeout)
{
	setParamInt("SYS_FAILURE_EN", 1);
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode); // warm up has_current / valid_current_mask, no injection active yet
	ASSERT_EQ(fd.getMotorFailures(), 0u);

	vehicle_command_s cmd{};
	cmd.timestamp = hrt_absolute_time();
	cmd.command = vehicle_command_s::VEHICLE_CMD_INJECT_FAILURE;
	cmd.param1 = vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR;
	cmd.param2 = vehicle_command_s::FAILURE_TYPE_STUCK;
	cmd.param3 = 1; // instance 1 -> ESC index 0
	_cmd_pub.publish(cmd);

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp; // would be fresh, but the injector blanks it before updateMotorStatus() runs
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures() & 0x01, 0x01u);
}

// SQE-FD-32 | update() return value (status_prev.value != _status.value)
// Given: FD_FAIL_R_TTRI 0.02s; a fresh FailureDetector
// When : update() with an attitude well within limits (no change expected) vs. update() sequence that flips the
//        roll flag from false to true
// Then : the no-change call returns false; the flipping call returns true
TEST_F(SqeFailureDetectorTest, FD32_UpdateReturnValue_ReflectsStatusChange)
{
	setParamFloat("FD_FAIL_R_TTRI", 0.02f);
	FailureDetector fd{nullptr};

	publishAttitude(0.f, 0.f);
	const bool changed_noop = fd.update(_status, _mode);
	EXPECT_FALSE(changed_noop);

	publishAttitude(70.f, 0.f);
	fd.update(_status, _mode);
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	publishAttitude(70.f, 0.f);
	const bool changed_trip = fd.update(_status, _mode);

	EXPECT_TRUE(changed_trip);
}

// SQE-FD-33 | FD-D29F — characterization (F-15)
// Given: following the FD23 steps 1-2 sequence (ESC0 timed out via telemetry timeout, motor flag true, timed-out
//        mask bit 0 set, under-current mask never engaged in this scenario)
// When : vehicle_status.arming_state is set to DISARMED and esc_status is republished
// Then : characterization (F-15) — motor flag becomes false (FD-D29F unconditionally resets it), but
//        getMotorFailures() still reports 0x01: the disarmed reset block only clears
//        _motor_failure_esc_under_current_mask and the undercurrent start times, never
//        _motor_failure_esc_timed_out_mask, so the timed-out bit survives in getMotorFailures() even though the
//        flags.motor bit the detector reports is false
TEST_F(SqeFailureDetectorTest, FD33_DisarmAfterTimeout_MotorFlagClearsButTimedOutMaskSurvivesCharacterization)
{
	setParamInt("FD_ESCS_EN", 0);
	FailureDetector fd{nullptr};

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_TRUE(fd.getStatusFlags().motor);
	ASSERT_EQ(fd.getMotorFailures() & 0x01, 0x01u);

	_status.arming_state = vehicle_status_s::ARMING_STATE_DISARMED;
	esc.timestamp = hrt_absolute_time();
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor);
	EXPECT_EQ(fd.getMotorFailures() & 0x01, 0x01u);
}

// SQE-FD-36 | FD-D38F (3rd operand — esc_timed_out True — P10 gap closure, DataValidatorGroup/FailureDetector
//            coverage iteration IT-1: L313's branch `!esc_timed_out` had never been exercised while the first
//            two operands (throttle_above_threshold, current_too_low) were also True). Note: both the timed-out
//            mask and the under-current mask are indexed by the same bit `(1 << i_esc)` (FailureDetector.hpp
//            `getMotorFailures()` ORs the two masks together), so for a single ESC (i_esc=0) they are
//            indistinguishable by bit value alone — this test instead isolates the under-current *path* by
//            checking it never runs via its own internal state machine (the start-time timer), using a second
//            ESC slot (i_esc=1, untouched/healthy) as a control to confirm the overall motor flag's cause.
// Given: FD_ACT_EN 1, FD_ACT_MOT_THR 0.2, FD_ACT_MOT_C2T 2.0; armed; ESC0 warmed up with current 5A (latches
//        has_current); actuator control 0.5 (above 0.2 threshold, throttle_above_threshold True)
// When : ESC0 is republished with esc_current 0.1A (low enough that current_too_low is True given the 0.5*2.0=1.0
//        threshold) AND a stale esc[0].timestamp (set via hrt_absolute_time() - 400ms, > the fixed 300ms ESC
//        telemetry timeout, so esc_timed_out is also True for this same sample) — all three FD-D38 operands'
//        raw inputs are True/True/True, but `!esc_timed_out` makes the evaluated 3rd operand False, so the
//        overall `&&` decision is False this call
// Then : the motor flag becomes true this same call — but via the independent ESC-telemetry-timeout path
//        (FD-D33, L288, `esc_was_valid && esc_timed_out && !flagged`, all True), not via under-current (confirmed
//        by then clearing the timeout condition with a fresh timestamp on the next call: if the under-current
//        timer had also been armed, the flag would stay latched past the fresh-timestamp call once its own 10ms+
//        elapsed; here it clears immediately because only the non-latching timed-out path, FD-D19/D34, was ever
//        active, proving the under-current branch at L313 truly took the False/else path this call)
TEST_F(SqeFailureDetectorTest, FD36_UnderCurrentConditionButEscAlreadyTimedOut_TimeoutPathFiresNotUnderCurrent)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 10'000); // 10s — deliberately far longer than this test's wall-clock span, so an
	// under-current timer (if armed) could not possibly expire within this test, isolating the under-current path.
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f;
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f; // warm up has_current / valid_current_mask
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_FALSE(fd.getStatusFlags().motor);

	// Low current (current_too_low True), throttle above threshold (True), but timestamp already stale
	// (esc_timed_out True) -> FD-D38's 3rd operand False -> overall decision False, under-current timer not armed.
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = hrt_absolute_time() - 400_ms;
	esc.esc[0].esc_current = 0.1f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	// Motor flag true this call, driven by the ESC-telemetry-timeout path (FD-D33), since FD_ACT_MOT_TOUT is 10s
	// and only ~0ms elapsed -> an armed under-current timer could not have expired yet even if it had been armed.
	ASSERT_TRUE(fd.getStatusFlags().motor);

	// Immediately republish with a fresh (non-stale) timestamp, current still low/throttle still above threshold.
	// FD-D34 (L292, `!esc_timed_out && flagged`) clears the timed-out bit. If the under-current path had also been
	// armed at the previous call despite FD-D38 being False (which would be the bug this test guards against), its
	// mask bit would stay latched (under-current is "never cleared" per L330) and the flag would stay true. It does
	// not: this proves L313 really took the else branch (L319) and never started the under-current timer.
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().motor) << "under-current timer must not have been armed by the FD-D38-False "
						 "sample — only the non-latching timeout path was active";
}

// SQE-FD-37 | FD-D41F (3rd operand — mask bit already set — P10 gap closure: L326's `== 0` branch had never been
//            exercised with the bit already latched from a prior call)
// Given: FD_ACT_EN 1, FD_ACT_MOT_THR 0.2, FD_ACT_MOT_C2T 2.0, FD_ACT_MOT_TOUT 10ms; armed; ESC0 warmed up; the
//        FD25-style sequence already latched the under-current mask bit (motor flag true, mask bit 0 set)
// When : another full under-current cycle is run (timer resets to 0 since the condition momentarily breaks when
//        republishing a sample with throttle below threshold, FD-D40 True; then low current/high throttle resumes
//        and a new timer runs to its own expiry) — by the time L324-326's 3-operand decision is re-evaluated with
//        the new timer having just expired, the under-current mask bit from the *first* latch is already 1, so
//        FD-D41's 3rd operand `(mask & bit) == 0` is False this time (the opposite of FD25, which only ever
//        exercises the bit-was-0 True case)
// Then : getMotorFailures() still reports the bit as set (it was already set — latched, "never cleared" per the
//        source comment at L330) and the assignment at L328 is a provable no-op on an already-1 bit (idempotent
//        OR) — confirmed by checking the flag/mask are unchanged across the second expiry, and by the fact that
//        the second cycle's own deliberate timer-reset step (FD-D40) proves a genuinely new timer instance ran
//        and expired independently of the first latch
TEST_F(SqeFailureDetectorTest, FD37_UnderCurrentMaskAlreadyLatched_SecondExpiryIsNoOp)
{
	setParamInt("FD_ESCS_EN", 0);
	setParamFloat("FD_ACT_MOT_THR", 0.2f);
	setParamFloat("FD_ACT_MOT_C2T", 2.0f);
	setParamInt("FD_ACT_MOT_TOUT", 10);
	FailureDetector fd{nullptr};

	actuator_motors_s act{};
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f;
	_act_pub.publish(act);

	esc_status_s esc{};
	esc.timestamp = hrt_absolute_time();
	esc.esc_count = 1;
	esc.esc[0].actuator_function = actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 5.f; // warm up has_current
	_esc_pub.publish(esc);
	fd.update(_status, _mode);
	ASSERT_FALSE(fd.getStatusFlags().motor);

	// First latch: low current, throttle above threshold -> start timer -> wait > timeout -> mask bit sets (FD-D41 T)
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	esc.esc[0].esc_current = 0.1f;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	ASSERT_TRUE(fd.getStatusFlags().motor);
	ASSERT_EQ(fd.getMotorFailures() & 0x01, 0x01u);
	const uint16_t failures_after_first_latch = fd.getMotorFailures();

	// Second cycle: throttle drops (resets the start-time timer per FD-D40), then low current resumes and a new
	// timer runs to expiry -> FD-D41 is re-evaluated with the mask bit already 1 -> 3rd operand False this time.
	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.f; // below threshold -> throttle_above_threshold False -> resets timer (FD-D40)
	_act_pub.publish(act);
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	act.timestamp = hrt_absolute_time();
	act.control[0] = 0.5f; // above threshold again -> restarts the under-current timer from this call
	_act_pub.publish(act);
	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	esc.timestamp = hrt_absolute_time();
	esc.esc[0].timestamp = esc.timestamp;
	_esc_pub.publish(esc);
	fd.update(_status, _mode);

	// Mask/flag unchanged across the second expiry — idempotent OR on an already-set bit, FD-D41's False 3rd operand.
	EXPECT_EQ(fd.getMotorFailures(), failures_after_first_latch);
	EXPECT_TRUE(fd.getStatusFlags().motor);
}
