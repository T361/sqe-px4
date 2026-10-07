/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : MulticopterLandDetector.cpp (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional — the detector reads MPC_* / LNDMC_* parameters and 5 uORB topics.
 * Test double       : TestableMcLandDetector, a test subclass (seam). It re-exports the protected decision methods
 *                      and gives write access to the protected inputs that LandDetector::Run() normally fills from
 *                      uORB (armed flag, local position, acceleration, angular velocity, hysteresis states). It does
 *                      not override any production method, so every decision executed here is the shipped code.
 *                      Hysteresis objects are driven with explicit timestamps, so the tests are deterministic; the only
 *                      wall-clock wait is the production 8 s minimum-thrust hysteresis (MC07), used as a lower bound.
 *                      LandDetector::Run() itself (private) is covered through the real work queue in
 *                      SqeLandDetectorRunTest.cpp.
 * Decisions covered : see test_inventory.csv (MLD-D01..MLD-D39); MC/DC rows for MLD-D29, MLD-D35, MLD-D37 in the
 *                      workbook's MC/DC sheet (supplementary to the DataValidatorGroup analysis)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/topics/hover_thrust_estimate.h>
#include <uORB/topics/takeoff_status.h>
#include <uORB/topics/trajectory_setpoint.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/vehicle_thrust_setpoint.h>

#include "MulticopterLandDetector.h"

using namespace time_literals;
using matrix::Vector3f;

class TestableMcLandDetector : public land_detector::MulticopterLandDetector
{
public:
	using MulticopterLandDetector::_update_params;
	using MulticopterLandDetector::_update_topics;
	using MulticopterLandDetector::_get_ground_contact_state;
	using MulticopterLandDetector::_get_maybe_landed_state;
	using MulticopterLandDetector::_get_landed_state;
	using MulticopterLandDetector::_get_freefall_state;
	using MulticopterLandDetector::_get_ground_effect_state;
	using MulticopterLandDetector::_set_hysteresis_factor;
	using MulticopterLandDetector::_get_in_descend;
	using MulticopterLandDetector::_get_has_low_throttle;
	using MulticopterLandDetector::_get_horizontal_movement;
	using MulticopterLandDetector::_get_vertical_movement;
	using MulticopterLandDetector::_get_rotational_movement;
	using MulticopterLandDetector::_get_close_to_ground_or_skipped_check;

	void setArmed(bool armed) { _armed = armed; }
	vehicle_local_position_s &lpos() { return _vehicle_local_position; }
	void setAcceleration(const Vector3f &a) { _acceleration = a; }
	void setAngularVelocity(const Vector3f &w) { _angular_velocity = w; }
	void setDistBottomObservable(bool o) { _dist_bottom_is_observable = o; }
	void forceLanded(bool s) { _landed_hysteresis.set_state_and_update(s, hrt_absolute_time()); }
	void forceMaybeLanded(bool s) { _maybe_landed_hysteresis.set_state_and_update(s, hrt_absolute_time()); }
	void forceGroundContact(bool s) { _ground_contact_hysteresis.set_state_and_update(s, hrt_absolute_time()); }
	void forceFreefall(bool s) { _freefall_hysteresis.set_state_and_update(s, hrt_absolute_time()); }
	systemlib::Hysteresis &groundContactHysteresis() { return _ground_contact_hysteresis; }
	systemlib::Hysteresis &landedHysteresis() { return _landed_hysteresis; }
};

class SqeMcLandDetectorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 3_s);

		setFloat("MPC_THR_MIN", 0.12f);
		setFloat("MPC_THR_HOVER", 0.5f);
		setFloat("MPC_MANTHR_MIN", 0.08f);
		setInt("MPC_USE_HTE", 0);
		setFloat("MPC_LAND_SPEED", 0.7f);
		setFloat("MPC_LAND_CRWL", 0.3f);
		setFloat("LNDMC_Z_VEL_MAX", 0.2f);
		setFloat("LNDMC_XY_VEL_MAX", 1.5f);
		setFloat("LNDMC_ROT_MAX", 20.f);
		setFloat("LNDMC_ALT_GND", 2.f);
		setFloat("LNDMC_TRIG_TIME", 0.3f);

		// neutral latest samples so a new detector never inherits a previous test's topics
		pubThrust(0.5f);
		pubControlMode(false);
		pubHte(0.5f, false, 0);
		pubTakeoff(takeoff_status_s::TAKEOFF_STATE_DISARMED);
		pubTrajectoryVz(NAN);
	}

	static void setInt(const char *n, int32_t v) { ASSERT_EQ(param_set(param_find(n), &v), PX4_OK) << n; }
	static void setFloat(const char *n, float v) { ASSERT_EQ(param_set(param_find(n), &v), PX4_OK) << n; }
	static float getFloat(const char *n) { float v = NAN; param_get(param_find(n), &v); return v; }

	void pubThrust(float throttle)
	{
		vehicle_thrust_setpoint_s s{};
		s.timestamp = hrt_absolute_time();
		s.xyz[2] = -throttle;
		_thrust_pub.publish(s);
	}
	void pubControlMode(bool climb_rate)
	{
		vehicle_control_mode_s m{};
		m.timestamp = hrt_absolute_time();
		m.flag_control_climb_rate_enabled = climb_rate;
		_mode_pub.publish(m);
	}
	void pubHte(float hover, bool valid, hrt_abstime ts)
	{
		hover_thrust_estimate_s h{};
		h.timestamp = ts;
		h.hover_thrust = hover;
		h.valid = valid;
		_hte_pub.publish(h);
	}
	void pubTakeoff(uint8_t state)
	{
		takeoff_status_s t{};
		t.timestamp = hrt_absolute_time();
		t.takeoff_state = state;
		_takeoff_pub.publish(t);
	}
	void pubTrajectoryVz(float vz)
	{
		trajectory_setpoint_s t{};
		t.timestamp = hrt_absolute_time();
		t.velocity[0] = t.velocity[1] = NAN;
		t.velocity[2] = vz;
		_traj_pub.publish(t);
	}

	// fresh local position, nothing valid, vehicle at rest
	static vehicle_local_position_s freshLpos()
	{
		vehicle_local_position_s p{};
		p.timestamp = hrt_absolute_time();
		return p;
	}

	// a constructed detector whose parameters were read once (what Run() does on its first cycle)
	static void prepare(TestableMcLandDetector &ld)
	{
		ld._update_params();
		ld._update_topics();
	}

	uORB::Publication<vehicle_thrust_setpoint_s> _thrust_pub{ORB_ID(vehicle_thrust_setpoint)};
	uORB::Publication<vehicle_control_mode_s> _mode_pub{ORB_ID(vehicle_control_mode)};
	uORB::Publication<hover_thrust_estimate_s> _hte_pub{ORB_ID(hover_thrust_estimate)};
	uORB::Publication<takeoff_status_s> _takeoff_pub{ORB_ID(takeoff_status)};
	uORB::Publication<trajectory_setpoint_s> _traj_pub{ORB_ID(trajectory_setpoint)};
};

// ===================================================================================================================
// _update_params / _update_topics
// ===================================================================================================================

// SQE-MLD-01 | MLD-D07 F · MLD-D08 F · MLD-D09 T(1st operand: HTE disabled)
// Given: LNDMC_Z_VEL_MAX 0.2, MPC_LAND_CRWL 0.3, MPC_LAND_SPEED 0.7 -> upper limit min(0.3,0.7)/1.2 = 0.25
// When : _update_params()
// Then : 0.2 is inside the limit, so the stored parameter is left untouched
TEST_F(SqeMcLandDetectorTest, MLD01_VerticalSpeedLimitInsideBound_ParameterUnchanged)
{
	TestableMcLandDetector ld;
	ld._update_params();
	EXPECT_FLOAT_EQ(getFloat("LNDMC_Z_VEL_MAX"), 0.2f);
}

// SQE-MLD-02 | MLD-D07 T (limit exceeded -> clamped and committed) · MLD-D13 F (effect of the clamp)
// Given: LNDMC_Z_VEL_MAX 0.5 above the 0.25 bound
// When : construct, _update_params()
// Then : the stored parameter is rewritten to 0.25 (0.3/1.2) and the detector uses it: a 0.3 m/s vertical speed,
//        which would be "still" under the configured 0.5, now counts as vertical movement
TEST_F(SqeMcLandDetectorTest, MLD02_VerticalSpeedLimitAboveBound_ClampedAndCommitted)
{
	setFloat("LNDMC_Z_VEL_MAX", 0.5f);
	TestableMcLandDetector ld;
	prepare(ld);

	EXPECT_NEAR(getFloat("LNDMC_Z_VEL_MAX"), 0.25f, 1e-5f);

	ld.setArmed(true);
	ld.forceLanded(false); // the detector boots "landed" (widened x2.5 threshold); evaluate the in-air threshold
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;
	ld.lpos().vz = 0.3f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_vertical_movement());
}

// SQE-MLD-03 | MLD-D21 F(0.3) · MLD-D22 T at the boundary and F just above · MLD-D19 F
// Given: HTE disabled; MPC_THR_MIN 0.12, MPC_THR_HOVER 0.5 -> low-throttle limit 0.12 + 0.38*0.3 = 0.234
// When : ground-contact evaluation with throttle 0.234 (exactly the limit; "<=") and with 0.24
// Then : 0.234 is low throttle, 0.24 is not
TEST_F(SqeMcLandDetectorTest, MLD03_LowThrottleLimit_BoundaryInclusive)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();

	pubThrust(0.12f + (0.5f - 0.12f) * 0.3f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());

	pubThrust(0.24f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_has_low_throttle());
}

// SQE-MLD-04 | MLD-D03 T · MLD-D04 T · MLD-D05 T · MLD-D09 T(2nd operand) then F(F∨F) · MLD-D19 T · MLD-D21 T(0.6)
// Given: MPC_USE_HTE 1, MPC_THR_HOVER 0.5; a valid hover-thrust estimate of 0.3 stamped now
// When : _update_params() (first: initialises hover from the parameter), _update_topics() takes the estimate, then
//        _update_params() again
// Then : the second parameter read does NOT overwrite the estimate: with hover 0.3 and the relaxed 60 % factor the
//        low-throttle limit is 0.12 + 0.18*0.6 = 0.228, so throttle 0.3 is not low (with hover 0.5 it would be:
//        0.12 + 0.38*0.6 = 0.348)
TEST_F(SqeMcLandDetectorTest, MLD04_HoverThrustEstimate_UsedAndNotOverwrittenByParameter)
{
	setInt("MPC_USE_HTE", 1);
	TestableMcLandDetector ld;
	ld._update_params();

	pubHte(0.3f, true, hrt_absolute_time());
	pubThrust(0.3f);
	ld._update_topics();
	ld._update_params();

	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_has_low_throttle());

	pubThrust(0.22f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());
}

// SQE-MLD-05 | MLD-D05 F (estimate flagged invalid) · MLD-D04 F (no new estimate)
// Given: MPC_USE_HTE 1; an INVALID estimate (hover 0.3, valid=false)
// When : _update_topics() twice
// Then : the invalid estimate is ignored: hover stays at the 0.5 parameter and the HTE is not "recently valid", so the
//        strict 30 % limit applies: 0.12 + 0.38*0.3 = 0.234 -> throttle 0.23 low, 0.24 not low
TEST_F(SqeMcLandDetectorTest, MLD05_InvalidHoverThrustEstimate_Ignored)
{
	setInt("MPC_USE_HTE", 1);
	TestableMcLandDetector ld;
	ld._update_params();
	pubHte(0.3f, false, hrt_absolute_time());
	ld._update_topics();
	ld._update_topics();

	ld.setArmed(true);
	ld.lpos() = freshLpos();
	pubThrust(0.23f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());

	pubThrust(0.24f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_has_low_throttle());
}

// SQE-MLD-06 | MLD-D19 F (estimate older than 1 s) · MLD-D20 T(1st operand: not descending)
// Given: MPC_USE_HTE 1; a valid estimate (hover 0.3) whose timestamp is 2 s old
// When : ground-contact evaluation with throttle 0.2
// Then : the hover value 0.3 is taken, but the estimate is not recent, so the strict 30 % factor applies:
//        0.12 + 0.18*0.3 = 0.174 -> throttle 0.2 is NOT low (under the 60 % factor it would be: 0.228)
TEST_F(SqeMcLandDetectorTest, MLD06_StaleHoverThrustEstimate_StrictFactor)
{
	setInt("MPC_USE_HTE", 1);
	TestableMcLandDetector ld;
	ld._update_params();
	pubHte(0.3f, true, hrt_absolute_time() - 2_s);
	pubThrust(0.2f);
	ld._update_topics();

	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_has_low_throttle());
}

// SQE-MLD-07 | MLD-D20 F∨T (descending, estimate valid) then F∨F (estimate became invalid: previous validity
//              kept) · MLD-D25 T∧T
// Given: MPC_USE_HTE 1, climb-rate control on, a recent valid estimate (hover 0.3), commanded descent 0.5 m/s
//        (>= 1.1 * LNDMC_Z_VEL_MAX = 0.22)
// When : first evaluation (estimate valid, starts descending); the estimate then goes stale (re-published 2 s old)
//        and the detector is evaluated again while still descending
// Then : the detector keeps treating the estimate as valid during the descent (relaxed limit 0.228 stays in force):
//        throttle 0.2 is low in the second evaluation; positive control: a fresh detector with the stale estimate and
//        no descent in progress uses the strict limit 0.174 and reports throttle 0.2 as NOT low
TEST_F(SqeMcLandDetectorTest, MLD07_EstimateInvalidDuringDescent_ValidityLatched)
{
	setInt("MPC_USE_HTE", 1);
	pubControlMode(true);
	TestableMcLandDetector ld;
	ld._update_params();
	pubHte(0.3f, true, hrt_absolute_time());
	pubTrajectoryVz(0.5f);
	pubThrust(0.2f);
	ld._update_topics();
	ld.setArmed(true);
	ld.lpos() = freshLpos();

	ld._get_ground_contact_state();
	ASSERT_TRUE(ld._get_in_descend());

	// still descending, estimate still recent: validity re-evaluated True (MLD-D20 2nd operand)
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());

	pubHte(0.3f, true, hrt_absolute_time() - 2_s);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());

	TestableMcLandDetector control;
	control._update_params();
	control._update_topics();
	control.setArmed(true);
	control.lpos() = freshLpos();
	pubTrajectoryVz(0.f);
	control._update_topics();
	control._get_ground_contact_state();
	EXPECT_FALSE(control._get_has_low_throttle());
}

// SQE-MLD-08 | MLD-D10 T/F with the boundary (specific force norm < 2 m/s^2 means free fall)
// Given: accelerations of norm 1.0, exactly 2.0 and 9.81 m/s^2
// When : _get_freefall_state()
// Then : only 1.0 is free fall; exactly 2.0 is not (strict "<")
TEST_F(SqeMcLandDetectorTest, MLD08_Freefall_NormBelowTwoBoundaryStrict)
{
	TestableMcLandDetector ld;

	ld.setAcceleration(Vector3f{1.f, 0.f, 0.f});
	EXPECT_TRUE(ld._get_freefall_state());
	ld.setAcceleration(Vector3f{0.f, 0.f, 2.f});
	EXPECT_FALSE(ld._get_freefall_state());
	ld.setAcceleration(Vector3f{0.f, 0.f, -9.81f});
	EXPECT_FALSE(ld._get_freefall_state());
}

// ===================================================================================================================
// _get_ground_contact_state — structural cases
// ===================================================================================================================

// SQE-MLD-09 | MLD-D11 F · MLD-D15 F(1st operand) · MLD-D17 F(1st operand)
// Given: armed; local position 2 s old (but all validity flags set, vehicle still)
// When : ground-contact evaluation
// Then : without a recent position estimate the detector assumes vertical movement, horizontal movement unknown
//        (false), not below ground-effect height, and therefore reports no ground contact
TEST_F(SqeMcLandDetectorTest, MLD09_StaleLocalPosition_AssumesVerticalMovement)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().timestamp = hrt_absolute_time() - 2_s;
	ld.lpos().v_z_valid = ld.lpos().z_valid = ld.lpos().v_xy_valid = ld.lpos().dist_bottom_valid = true;
	ld.lpos().dist_bottom = 0.5f;

	EXPECT_FALSE(ld._get_ground_contact_state());
	EXPECT_TRUE(ld._get_vertical_movement());
	EXPECT_FALSE(ld._get_horizontal_movement());
	ld._get_ground_effect_state();
	EXPECT_FALSE(ld._get_ground_effect_state());
}

// SQE-MLD-10 | MLD-D13 T then F (incl. boundary) · MLD-D14 F(1st operand)
// Given: armed, fresh position with only v_z valid
// When : vz 0.1 m/s, then 0.3 m/s, then exactly 0.2 m/s (limit 0.2)
// Then : 0.1 is "no vertical movement"; 0.3 and the boundary value 0.2 are movement (z-derivative fallback unavailable)
TEST_F(SqeMcLandDetectorTest, MLD10_VerticalSpeed_FromVz)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.forceLanded(false);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;

	ld.lpos().vz = 0.1f;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_vertical_movement());

	ld.lpos().vz = 0.3f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_vertical_movement());

	// boundary: |vz| exactly equal to the 0.2 limit is movement (strict "<" for "still")
	ld.lpos().vz = 0.2f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_vertical_movement());
}

// SQE-MLD-11 | MLD-D13 F(1st operand) · MLD-D14 T then F(2nd operand)
// Given: armed, fresh position, v_z invalid but z valid
// When : z_deriv -0.1 m/s, then z_deriv -0.5 m/s
// Then : the z-derivative fallback detects "still" for -0.1 and movement for -0.5
TEST_F(SqeMcLandDetectorTest, MLD11_VerticalSpeed_FallsBackToZDerivative)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.forceLanded(false);
	ld.lpos() = freshLpos();
	ld.lpos().z_valid = true;

	ld.lpos().z_deriv = -0.1f;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_vertical_movement());

	ld.lpos().z_deriv = -0.5f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_vertical_movement());
}

// SQE-MLD-12 | MLD-D12 T then F (vertical threshold widened x2.5 while landed)
// Given: armed, fresh position, vz 0.4 m/s (limit 0.2, widened 0.5)
// When : evaluated with the landed hysteresis true, then false
// Then : landed -> still (0.4 < 0.5); not landed -> moving (0.4 >= 0.2)
TEST_F(SqeMcLandDetectorTest, MLD12_LandedState_WidensVerticalThreshold)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;
	ld.lpos().vz = 0.4f;

	ld.forceLanded(true);
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_vertical_movement());

	ld.forceLanded(false);
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_vertical_movement());
}

// SQE-MLD-13 | MLD-D15 T then F(2nd operand)
// Given: armed, fresh position, LNDMC_XY_VEL_MAX 1.5
// When : v_xy valid with (1.0, 1.2) m/s (norm 1.56), with (1.0, 1.0) (norm 1.41), then v_xy invalid
// Then : 1.56 > 1.5 is horizontal movement; 1.41 is not; an invalid v_xy is treated as "not known" (false)
TEST_F(SqeMcLandDetectorTest, MLD13_HorizontalMovement_VelocityNormAgainstLimit)
{
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_xy_valid = true;

	ld.lpos().vx = 1.0f; ld.lpos().vy = 1.2f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_horizontal_movement());

	ld.lpos().vy = 1.0f;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_horizontal_movement());

	ld.lpos().v_xy_valid = false;
	ld.lpos().vy = 5.0f;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_horizontal_movement());
}

// SQE-MLD-14 | MLD-D17 T∧T∧T then F(2nd operand) and F(3rd operand) · MLD-D18 T/F — observed through ground effect
// Given: takeoff state FLIGHT (so "below ground-effect height" alone decides ground effect), LNDMC_ALT_GND 2 m
// When : distance to ground 1.5 m (valid), 2.5 m, exactly 2.0 m, 1.5 m but invalid, and 1.5 m valid with LNDMC_ALT_GND 0
// Then : ground effect only for the valid 1.5 m reading with a positive ground-effect altitude (2.0 m is not below 2 m)
TEST_F(SqeMcLandDetectorTest, MLD14_BelowGroundEffectHeight_RequiresValidDistanceAndPositiveParameter)
{
	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_FLIGHT);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().dist_bottom_valid = true;

	ld.lpos().dist_bottom = 1.5f;
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_ground_effect_state());

	ld.lpos().dist_bottom = 2.5f;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_ground_effect_state());

	ld.lpos().dist_bottom = 2.f; // boundary: exactly LNDMC_ALT_GND is not "below" (strict "<")
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_ground_effect_state());

	ld.lpos().dist_bottom = 1.5f;
	ld.lpos().dist_bottom_valid = false;
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_ground_effect_state());

	setFloat("LNDMC_ALT_GND", 0.f);
	TestableMcLandDetector off;
	prepare(off);
	off.setArmed(true);
	off.lpos() = freshLpos();
	off.lpos().dist_bottom_valid = true;
	off.lpos().dist_bottom = 1.5f;
	off._get_ground_contact_state();
	EXPECT_FALSE(off._get_ground_effect_state());
}

// SQE-MLD-15 | MLD-D23 T · MLD-D24 T · MLD-D25 F(1st operand: NaN) and T∧F(0 m/s) · MLD-D26 T
// Given: armed, climb-rate control on, low throttle, vehicle still, landing states false
// When : the commanded vertical velocity is NaN, then 0 m/s (hover), then 0.5 m/s (descending)
// Then : ground contact requires a commanded descent: NaN and hover give no ground contact; descending gives contact
TEST_F(SqeMcLandDetectorTest, MLD15_ClimbRateControl_GroundContactRequiresCommandedDescent)
{
	pubControlMode(true);
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;
	ld.forceLanded(false);
	ld.forceMaybeLanded(false);

	pubTrajectoryVz(NAN);
	ld._update_topics();
	EXPECT_FALSE(ld._get_ground_contact_state());
	EXPECT_FALSE(ld._get_in_descend());

	pubTrajectoryVz(0.f);
	ld._update_topics();
	EXPECT_FALSE(ld._get_ground_contact_state());

	pubTrajectoryVz(0.5f);
	ld._update_topics();
	EXPECT_TRUE(ld._get_ground_contact_state());
	EXPECT_TRUE(ld._get_in_descend());
}

// SQE-MLD-16 | MLD-D25 boundary (velocity exactly 1.1 * LNDMC_Z_VEL_MAX counts as descending, ">=")
// Given: climb-rate control on, LNDMC_Z_VEL_MAX 0.2
// When : commanded vz = 1.1f * 0.2f, then a value just below
// Then : the exact product is "in descend", the value below is not
TEST_F(SqeMcLandDetectorTest, MLD16_DescendThreshold_BoundaryInclusive)
{
	pubControlMode(true);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();

	pubTrajectoryVz(1.1f * 0.2f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_in_descend());

	pubTrajectoryVz(0.21f);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_in_descend());
}

// SQE-MLD-17 | MLD-D24 F (no new trajectory setpoint: previous descend decision kept) · MLD-D26 F(1st: maybe
//              landed) · F(2nd: landed only) · T∧T (positive control)
// Given: climb-rate control on, low throttle, commanded descent then no further setpoints; maybe-landed true
// When : second evaluation without a new setpoint, then evaluation with hover command while maybe-landed
// Then : the descend flag is kept from the last setpoint; once "maybe landed" (or "landed") the descent is no longer
//        required, so hover (vz 0) still gives ground contact; with neither state it does not
TEST_F(SqeMcLandDetectorTest, MLD17_NoNewSetpointKeepsDescend_MaybeLandedDropsRequirement)
{
	pubControlMode(true);
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;
	ld.forceLanded(false);
	ld.forceMaybeLanded(false);

	pubTrajectoryVz(0.5f);
	ld._update_topics();
	ld._get_ground_contact_state();
	ASSERT_TRUE(ld._get_in_descend());
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_in_descend());

	pubTrajectoryVz(0.f);
	ld._update_topics();
	ld.forceMaybeLanded(true);
	EXPECT_TRUE(ld._get_ground_contact_state());
	EXPECT_FALSE(ld._get_in_descend());

	// landed while maybe-landed is false: the requirement is lifted by the second operand as well
	ld.forceMaybeLanded(false);
	ld.forceLanded(true);
	EXPECT_TRUE(ld._get_ground_contact_state());

	// positive control: neither state -> hover without descent gives no ground contact
	ld.forceLanded(false);
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// SQE-MLD-18 | MLD-D23 F (manual thrust: in_descend forced false, ground contact = low throttle only)
// Given: climb-rate control off, a descent command present, low throttle, vehicle still
// When : ground-contact evaluation
// Then : in_descend is false (not used without climb-rate control) and ground contact follows low throttle alone
TEST_F(SqeMcLandDetectorTest, MLD18_ManualThrust_InDescendFalseContactFromThrottle)
{
	pubControlMode(false);
	pubThrust(0.1f);
	pubTrajectoryVz(0.5f);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;

	EXPECT_TRUE(ld._get_ground_contact_state());
	EXPECT_FALSE(ld._get_in_descend());
}

// SQE-MLD-19 | MLD-D27 T(1st) / T(2nd) / F · MLD-D28 T(close) / F · MLD-D38 T/F · MLD-D39 T/F
// Given: armed, low throttle, still
// When : (a) distance not observable; (b) observable but distance invalid; (c) observable, valid, 0.5 m;
//        (d) observable, valid, 1.5 m; (e) exactly 1.0 m
// Then : the close-to-ground check is skipped in (a)/(b), passes in (c) (0.5 < 1.0 m) and fails in (d) and at the
//        boundary (e), which removes ground contact
TEST_F(SqeMcLandDetectorTest, MLD19_CloseToGroundCheck_SkippedOrEnforced)
{
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld.lpos().v_z_valid = true;

	ld.setDistBottomObservable(false);
	EXPECT_TRUE(ld._get_ground_contact_state());
	EXPECT_TRUE(ld._get_close_to_ground_or_skipped_check());

	ld.setDistBottomObservable(true);
	ld.lpos().dist_bottom_valid = false;
	EXPECT_TRUE(ld._get_ground_contact_state());

	ld.lpos().dist_bottom_valid = true;
	ld.lpos().dist_bottom = 0.5f;
	EXPECT_TRUE(ld._get_ground_contact_state());

	ld.lpos().dist_bottom = 1.5f;
	EXPECT_FALSE(ld._get_ground_contact_state());
	EXPECT_FALSE(ld._get_close_to_ground_or_skipped_check());

	ld.lpos().dist_bottom = 1.0f; // boundary: exactly DIST_FROM_GROUND_THRESHOLD is not "close" (strict "<")
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// ===================================================================================================================
// MC/DC — MLD-D29: return !_armed || (close_or_skip && ground_contact && !horizontal && !vertical)
//   A = !_armed, B = close_or_skip, C = ground_contact (local), D = !horizontal, E = !vertical
// ===================================================================================================================

namespace
{
// baseline for D29 where B, C, D, E are all True with the vehicle armed
void d29Baseline(TestableMcLandDetector &ld)
{
	ld.setArmed(true);
	ld.forceLanded(false);
	ld.setDistBottomObservable(false);
	ld.lpos() = vehicle_local_position_s{};
	ld.lpos().timestamp = hrt_absolute_time();
	ld.lpos().v_z_valid = true;
}
}

// SQE-MLD-MC-01 | D29 A=T (B..E not evaluated) -> T
// Given: disarmed; throttle high (would remove ground contact)
// Then : disarmed alone means ground contact
TEST_F(SqeMcLandDetectorTest, MLDMC01_D29_Disarmed_GroundContact)
{
	pubThrust(0.6f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	ld.setArmed(false);
	EXPECT_TRUE(ld._get_ground_contact_state());
}

// SQE-MLD-MC-02 | D29 A=F B=T C=T D=T E=T -> T (baseline)
TEST_F(SqeMcLandDetectorTest, MLDMC02_D29_AllConditionsMet_GroundContact)
{
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	EXPECT_TRUE(ld._get_ground_contact_state());
}

// SQE-MLD-MC-03 | D29 A=F B=F (C..E not evaluated) -> F   [B pair with MC-02]
// Given: baseline but distance observable and valid at 1.5 m (not within 1 m)
TEST_F(SqeMcLandDetectorTest, MLDMC03_D29_NotCloseToGround_NoContact)
{
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	ld.setDistBottomObservable(true);
	ld.lpos().dist_bottom_valid = true;
	ld.lpos().dist_bottom = 1.5f;
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// SQE-MLD-MC-04 | D29 A=F B=T C=F (D, E not evaluated) -> F   [C pair with MC-02; A pair with MC-01]
// Given: baseline but throttle 0.6 (not low)
TEST_F(SqeMcLandDetectorTest, MLDMC04_D29_HighThrottle_NoContact)
{
	pubThrust(0.6f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// SQE-MLD-MC-05 | D29 A=F B=T C=T D=F (E not evaluated) -> F   [D pair with MC-02]
// Given: baseline but horizontal velocity (2, 0) m/s valid
TEST_F(SqeMcLandDetectorTest, MLDMC05_D29_HorizontalMovement_NoContact)
{
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	ld.lpos().v_xy_valid = true;
	ld.lpos().vx = 2.f;
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// SQE-MLD-MC-06 | D29 A=F B=T C=T D=T E=F -> F   [E pair with MC-02]
// Given: baseline but vz 1 m/s
TEST_F(SqeMcLandDetectorTest, MLDMC06_D29_VerticalMovement_NoContact)
{
	pubThrust(0.1f);
	TestableMcLandDetector ld;
	prepare(ld);
	d29Baseline(ld);
	ld.lpos().vz = 1.f;
	EXPECT_FALSE(ld._get_ground_contact_state());
}

// ===================================================================================================================
// MC/DC — MLD-D35 (maybe landed):
//   return !_armed || (min_thrust && !freefall && !rotation && ((vert_est && gc_hyst) || (!vert_est && min_thrust_8s)))
//   A = !_armed, B = minimum_thrust_now, C = !freefall, D = !rotational_movement, E = vertical_estimate (appears twice,
//   coupled), F = ground_contact_hysteresis, G = minimum_thrust_8s_hysteresis
// ===================================================================================================================

namespace
{
// A=F B=T C=T D=T E=T F=T: armed, manual thrust (limit MPC_MANTHR_MIN + 0.01 = 0.09) with throttle 0.05,
// no freefall, no rotation, fresh position with valid vz, ground-contact hysteresis true
void d35Baseline(TestableMcLandDetector &ld)
{
	ld.setArmed(true);
	ld.forceFreefall(false);
	ld.forceLanded(false);
	ld.forceGroundContact(true);
	ld.setAngularVelocity(Vector3f{0.f, 0.f, 0.f});
	ld.lpos() = vehicle_local_position_s{};
	ld.lpos().timestamp = hrt_absolute_time();
	ld.lpos().v_z_valid = true;
}
}

// SQE-MLD-MC-07 | D35 A=T -> T   [A pair with MC-08]
TEST_F(SqeMcLandDetectorTest, MLDMC07_D35_Disarmed_MaybeLanded)
{
	pubThrust(0.6f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.setArmed(false);
	EXPECT_TRUE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-08 | D35 A=F B=F (rest not evaluated) -> F   [B pair with MC-09]
TEST_F(SqeMcLandDetectorTest, MLDMC08_D35_ThrustAboveMinimum_NotMaybeLanded)
{
	pubThrust(0.6f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	EXPECT_FALSE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-09 | D35 A=F B=T C=T D=T E=T F=T (G not evaluated) -> T   (baseline)
TEST_F(SqeMcLandDetectorTest, MLDMC09_D35_AllConditionsWithVerticalEstimate_MaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	EXPECT_TRUE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-10 | D35 A=F B=T C=F -> F   [C pair with MC-09]
TEST_F(SqeMcLandDetectorTest, MLDMC10_D35_Freefall_NotMaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.forceFreefall(true);
	EXPECT_FALSE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-11 | D35 A=F B=T C=T D=F -> F   [D pair with MC-09]
// Given: baseline with roll rate 0.6 rad/s (> radians(20) = 0.349)
TEST_F(SqeMcLandDetectorTest, MLDMC11_D35_Rotating_NotMaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.setAngularVelocity(Vector3f{0.6f, 0.f, 0.f});
	EXPECT_FALSE(ld._get_maybe_landed_state());
	EXPECT_TRUE(ld._get_rotational_movement());
}

// SQE-MLD-MC-12 | D35 A=F B=T C=T D=T E=T F=F (then E-branch false, G not evaluated since !E is False) -> F
//                 [F pair with MC-09]
TEST_F(SqeMcLandDetectorTest, MLDMC12_D35_NoGroundContactHysteresis_NotMaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.forceGroundContact(false);
	EXPECT_FALSE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-13 | D35 A=F B=T C=T D=T E=F (F not evaluated) G=F -> F   [E pair with MC-09; G pair with MC-14]
// Given: baseline but vz flagged invalid (no vertical estimate) and the 8 s minimum-thrust hysteresis not yet elapsed
TEST_F(SqeMcLandDetectorTest, MLDMC13_D35_NoVerticalEstimateMinThrustNotHeld_NotMaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.lpos().v_z_valid = false;
	EXPECT_FALSE(ld._get_maybe_landed_state());
}

// SQE-MLD-MC-14 | D35 A=F B=T C=T D=T E=F G=T -> T   [G pair with MC-13]
// Given: no vertical estimate; minimum thrust held for more than the production 8 s hysteresis (wall-clock wait, used
//        only as a lower bound — sleep_for never returns early)
// Then : after 8 s of minimum thrust the vehicle is "maybe landed" even without a vertical estimate
TEST_F(SqeMcLandDetectorTest, MLDMC14_D35_NoVerticalEstimateMinThrustHeld8s_MaybeLanded)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.lpos().v_z_valid = false;
	ASSERT_FALSE(ld._get_maybe_landed_state());

	std::this_thread::sleep_for(std::chrono::milliseconds(8100));
	ld.lpos().timestamp = hrt_absolute_time();
	EXPECT_TRUE(ld._get_maybe_landed_state());
}

// SQE-MLD-20 | MLD-D30 T (climb-rate limit 10 % of min..hover) · MLD-D31 T/F around 0.158
// Given: climb-rate control on: limit 0.12 + 0.38*0.1 = 0.158
// When : throttle 0.15 and 0.17 with every other maybe-landed condition met
// Then : maybe landed only for 0.15
TEST_F(SqeMcLandDetectorTest, MLD20_ClimbRateMinimumThrustLimit)
{
	pubControlMode(true);
	pubThrust(0.15f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	EXPECT_TRUE(ld._get_maybe_landed_state());

	pubThrust(0.17f);
	ld._update_topics();
	EXPECT_FALSE(ld._get_maybe_landed_state());
}

// SQE-MLD-21 | MLD-D32 T (rotation threshold widened x2.5 while landed) · MLD-D34 F(stale position)
// Given: roll rate 0.6 rad/s: above radians(20) = 0.349 but below 2.5 * 0.349 = 0.873
// When : landed hysteresis true, then false
// Then : no rotational movement while landed; rotation detected when not landed
TEST_F(SqeMcLandDetectorTest, MLD21_LandedState_WidensRotationThreshold)
{
	pubThrust(0.05f);
	TestableMcLandDetector ld;
	prepare(ld);
	d35Baseline(ld);
	ld.setAngularVelocity(Vector3f{0.6f, 0.f, 0.f});

	ld.forceLanded(true);
	ld._get_maybe_landed_state();
	EXPECT_FALSE(ld._get_rotational_movement());

	ld.forceLanded(false);
	ld.lpos().timestamp = hrt_absolute_time() - 2_s;
	ld._get_maybe_landed_state();
	EXPECT_TRUE(ld._get_rotational_movement());
}

// SQE-MLD-22 | MLD-D36 T(disarmed) / F∨F / F∨T
// When : _get_landed_state() disarmed; armed with maybe-landed hysteresis false; armed with it true
// Then : true, false, true
TEST_F(SqeMcLandDetectorTest, MLD22_LandedState_DisarmedOrMaybeLandedHeld)
{
	TestableMcLandDetector ld;
	ld.setArmed(false);
	ld.forceMaybeLanded(false);
	EXPECT_TRUE(ld._get_landed_state());

	ld.setArmed(true);
	EXPECT_FALSE(ld._get_landed_state());

	ld.forceMaybeLanded(true);
	EXPECT_TRUE(ld._get_landed_state());
}

// ===================================================================================================================
// MC/DC — MLD-D37 (ground effect):
//   return (in_descend && !horizontal) || (below_gnd && takeoff == FLIGHT) || takeoff == RAMPUP
//   P = in_descend, Q = !horizontal, R = below_gnd_effect_hgt, S = takeoff==FLIGHT, U = takeoff==RAMPUP
//   (S and U read the same variable: at most one can be True — masking form for that pair)
// ===================================================================================================================

namespace
{
// drives in_descend / horizontal / below from real inputs, then evaluates D37
bool groundEffect(TestableMcLandDetector &ld, bool descend, bool horizontal, bool below)
{
	ld.setArmed(true);
	ld.lpos() = vehicle_local_position_s{};
	ld.lpos().timestamp = hrt_absolute_time();
	ld.lpos().v_xy_valid = true;
	ld.lpos().vx = horizontal ? 2.f : 0.f;
	ld.lpos().dist_bottom_valid = below;
	ld.lpos().dist_bottom = 1.f;
	ld._get_ground_contact_state();
	EXPECT_EQ(ld._get_in_descend(), descend);
	return ld._get_ground_effect_state();
}
}

// SQE-MLD-MC-15 | D37 P=T Q=T -> T
TEST_F(SqeMcLandDetectorTest, MLDMC15_D37_DescendingNoDrift_GroundEffect)
{
	pubControlMode(true);
	pubTrajectoryVz(0.5f);
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_TRUE(groundEffect(ld, true, false, false));
}

// SQE-MLD-MC-16 | D37 P=F (Q n.e.) R=F (S n.e.) U=F -> F   [P pair with MC-15; U pair with MC-20]
TEST_F(SqeMcLandDetectorTest, MLDMC16_D37_NothingApplies_NoGroundEffect)
{
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_FALSE(groundEffect(ld, false, false, false));
}

// SQE-MLD-MC-17 | D37 P=T Q=F R=F U=F -> F   [Q pair with MC-15]
TEST_F(SqeMcLandDetectorTest, MLDMC17_D37_DescendingWithDrift_NoGroundEffect)
{
	pubControlMode(true);
	pubTrajectoryVz(0.5f);
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_FALSE(groundEffect(ld, true, true, false));
}

// SQE-MLD-MC-18 | D37 P=F R=T S=T (U n.e.) -> T
TEST_F(SqeMcLandDetectorTest, MLDMC18_D37_BelowHeightInFlight_GroundEffect)
{
	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_FLIGHT);
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_TRUE(groundEffect(ld, false, false, true));
}

// SQE-MLD-MC-19 | D37 P=F R=T S=F U=F (takeoff DISARMED) -> F   [S pair with MC-18]
TEST_F(SqeMcLandDetectorTest, MLDMC19_D37_BelowHeightNotInFlight_NoGroundEffect)
{
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_FALSE(groundEffect(ld, false, false, true));
}

// SQE-MLD-MC-20 | D37 P=F R=F (S n.e.) U=T (RAMPUP) -> T   [U pair with MC-16]
TEST_F(SqeMcLandDetectorTest, MLDMC20_D37_RampUp_GroundEffect)
{
	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_RAMPUP);
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_TRUE(groundEffect(ld, false, false, false));
}

// SQE-MLD-MC-21 | D37 P=F R=F (S n.e.) U=F, takeoff FLIGHT -> F   [R pair with MC-18 — same takeoff value]
TEST_F(SqeMcLandDetectorTest, MLDMC21_D37_InFlightAboveHeight_NoGroundEffect)
{
	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_FLIGHT);
	TestableMcLandDetector ld;
	prepare(ld);
	EXPECT_FALSE(groundEffect(ld, false, false, false));
}

// SQE-MLD-23 | _set_hysteresis_factor (factor 1 vs 3) — deterministic timing through explicit timestamps
// Given: LNDMC_TRIG_TIME 0.3 s -> ground-contact/maybe/landed hysteresis = 0.3/3 * factor
// When : factor 1, then factor 3; each time the ground-contact hysteresis is requested True at t and updated at
//        t + 0.09 s, t + 0.11 s, t + 0.29 s, t + 0.31 s
// Then : factor 1 switches between 0.09 s and 0.11 s (0.1 s); factor 3 between 0.29 s and 0.31 s (0.3 s)
TEST_F(SqeMcLandDetectorTest, MLD23_HysteresisFactor_ScalesLandingDelays)
{
	TestableMcLandDetector ld;
	prepare(ld);
	systemlib::Hysteresis &gc = ld.groundContactHysteresis();
	hrt_abstime t = hrt_absolute_time();

	ld._set_hysteresis_factor(1);
	gc.set_state_and_update(false, t);
	gc.set_state_and_update(true, t);
	gc.update(t + 90_ms);
	EXPECT_FALSE(gc.get_state());
	gc.update(t + 110_ms);
	EXPECT_TRUE(gc.get_state());

	t += 1_s;
	ld._set_hysteresis_factor(3);
	gc.set_state_and_update(false, t);
	gc.set_state_and_update(true, t);
	gc.update(t + 290_ms);
	EXPECT_FALSE(gc.get_state());
	gc.update(t + 310_ms);
	EXPECT_TRUE(gc.get_state());
}

// SQE-MLD-24 | MLD-D01 F · MLD-D02 F · MLD-D06 T then F · MLD-D03 F (HTE disabled: estimate not read)
// Given: HTE disabled; takeoff state RAMPUP, a valid hover estimate of 0.2 and throttle 0.2 published
// When : _update_topics() twice (no new messages before the second call), then ground contact / ground effect
// Then : the takeoff state is taken over (RAMPUP -> ground effect true); the estimate is ignored, so the limit is
//        built from the parameter hover 0.5 with the strict 30 % factor: 0.234 -> throttle 0.2 is low (had the
//        estimate been used, 0.12 + 0.08*0.6 = 0.168 and 0.2 would not be low)
TEST_F(SqeMcLandDetectorTest, MLD24_TopicsTakenOnceAndEstimateIgnoredWhenDisabled)
{
	TestableMcLandDetector ld;
	ld._update_params();
	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_RAMPUP);
	pubHte(0.2f, true, hrt_absolute_time());
	pubThrust(0.2f);
	ld._update_topics();
	ld._update_topics();

	ld.setArmed(true);
	ld.lpos() = freshLpos();
	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_ground_effect_state());
	// parameter hover 0.5 -> limit 0.234 -> 0.2 is low; with the ignored estimate (0.2 hover, 60 %) the limit would be
	// 0.12 + 0.08*0.6 = 0.168 and 0.2 would NOT be low
	EXPECT_TRUE(ld._get_has_low_throttle());
}

// SQE-MLD-25 | MLD-D20 T(1st operand: not descending) with the estimate turning invalid -> validity drops immediately
// Given: MPC_USE_HTE 1, manual thrust (not descending); a recent valid estimate (hover 0.3) -> relaxed limit 0.228
// When : evaluated once (valid), then the estimate is re-published 2 s old and the detector is evaluated again
// Then : with the valid estimate throttle 0.2 is low (0.2 <= 0.228); once the estimate is stale and the vehicle is not
//        descending, the strict 30 % limit 0.174 applies immediately and 0.2 is no longer low (the "keep previous
//        validity" latch is only for descents, cf. MLD-07)
TEST_F(SqeMcLandDetectorTest, MLD25_EstimateInvalidWhileNotDescending_StrictLimitImmediately)
{
	setInt("MPC_USE_HTE", 1);
	TestableMcLandDetector ld;
	ld._update_params();
	pubHte(0.3f, true, hrt_absolute_time());
	pubThrust(0.2f);
	ld._update_topics();
	ld.setArmed(true);
	ld.lpos() = freshLpos();

	ld._get_ground_contact_state();
	EXPECT_TRUE(ld._get_has_low_throttle());

	pubHte(0.3f, true, hrt_absolute_time() - 2_s);
	ld._update_topics();
	ld._get_ground_contact_state();
	EXPECT_FALSE(ld._get_in_descend());
	EXPECT_FALSE(ld._get_has_low_throttle());
}
