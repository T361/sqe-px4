/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : LandDetector.cpp — Run() and UpdateVehicleAtRest() (PX4-Autopilot v1.17.0, d6f12ad1), executed
 *                      with the real MulticopterLandDetector
 * Test level        : GTest functional with PX4's real work-queue manager. LandDetector::Run() is private and
 *                      inherited through a private base, so it cannot be called from a test; instead the detector is
 *                      started exactly as the module does (start() -> scheduled on the nav_and_controllers work queue,
 *                      plus a callback on vehicle_local_position). Inputs are published on uORB, outputs are observed
 *                      on the vehicle_land_detected topic and in the LND_FLIGHT_T_* parameters.
 * Test double       : RunnableMcLandDetector re-exports only request_stop() (protected in ModuleBase) so each test
 *                      can stop its own instance; nothing is overridden.
 * Platform setup    : SetUpTestSuite performs the part of px4::init_once() that the functional gtest runner skips
 *                      (hrt_work_queue_init(), hrt_init(), WorkQueueManagerStart()); param/uORB init is done by the runner.
 * Timing            : the work queue runs on wall-clock time (CONFIG_BOARD_NOLOCKSTEP). Every wait polls the output
 *                      topic until a predicate holds or a generous timeout expires; fixed sleeps are used only as lower
 *                      bounds ("at least X ms passed, so Y must not have happened yet / must have happened").
 * Decisions covered : see test_inventory.csv (LD-D01..LD-D20)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <thread>

#include <drivers/drv_hrt.h>
#include <hrt_work.h>
#include <parameters/param.h>
#include <px4_platform_common/px4_work_queue/WorkQueueManager.hpp>
#include <uORB/Publication.hpp>
#include <uORB/PublicationMulti.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/sensor_selection.h>
#include <uORB/topics/takeoff_status.h>
#include <uORB/topics/vehicle_acceleration.h>
#include <uORB/topics/vehicle_angular_velocity.h>
#include <uORB/topics/vehicle_control_mode.h>
#include <uORB/topics/vehicle_imu_status.h>
#include <uORB/topics/vehicle_land_detected.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_thrust_setpoint.h>

#include "MulticopterLandDetector.h"

using namespace time_literals;
using namespace std::chrono_literals;

class RunnableMcLandDetector : public land_detector::MulticopterLandDetector
{
public:
	using land_detector::LandDetector::request_stop;
};

namespace
{
uORB::PublicationMulti<vehicle_imu_status_s> *g_imu0{nullptr};
uORB::PublicationMulti<vehicle_imu_status_s> *g_imu1{nullptr};
}

class SqeLandDetectorRunTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		// what px4::init_once() does at boot and gtest_functional_main does not: the posix HRT callout thread and the
		// HRT lock (needed by ScheduleDelayed), then the work-queue manager
		hrt_work_queue_init();
		hrt_init();
		ASSERT_EQ(px4::WorkQueueManagerStart(), PX4_OK);
		g_imu0 = new uORB::PublicationMulti<vehicle_imu_status_s>(ORB_ID(vehicle_imu_status));
		vehicle_imu_status_s seed{};
		seed.timestamp = hrt_absolute_time();
		g_imu0->publish(seed);
		g_imu1 = new uORB::PublicationMulti<vehicle_imu_status_s>(ORB_ID(vehicle_imu_status));
		g_imu1->publish(seed);
	}

	static void TearDownTestSuite()
	{
		delete g_imu0;
		delete g_imu1;
		g_imu0 = g_imu1 = nullptr;
		px4::WorkQueueManagerStop();
	}

	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		setFloat("MPC_THR_MIN", 0.12f);
		setFloat("MPC_THR_HOVER", 0.5f);
		setFloat("MPC_MANTHR_MIN", 0.08f);
		setInt("MPC_USE_HTE", 0);
		setFloat("LNDMC_Z_VEL_MAX", 0.2f);
		setFloat("LNDMC_TRIG_TIME", 0.3f);
		setInt("LND_FLIGHT_T_HI", 0);
		setInt("LND_FLIGHT_T_LO", 0);

		_armed = false;
		_throttle = 0.05f;
		_vz = 0.f;
		_range_sensor = false;
		_dist_valid = false;
		_dist = 0.f;
		_accel_z = -9.81f;
		publishInputs();
		publishControlMode();
		pubTakeoff(takeoff_status_s::TAKEOFF_STATE_DISARMED);
		_t_start = hrt_absolute_time(); // messages older than this belong to a previous test's detector
	}

	static void setInt(const char *n, int32_t v) { ASSERT_EQ(param_set(param_find(n), &v), PX4_OK) << n; }
	static void setFloat(const char *n, float v) { ASSERT_EQ(param_set(param_find(n), &v), PX4_OK) << n; }

	// republish the controllable inputs (keeps the local position "recent" for the detector)
	void publishInputs()
	{
		actuator_armed_s a{};
		a.timestamp = hrt_absolute_time();
		a.armed = _armed;
		_armed_pub.publish(a);

		vehicle_thrust_setpoint_s t{};
		t.timestamp = hrt_absolute_time();
		t.xyz[2] = -_throttle;
		_thrust_pub.publish(t);

		vehicle_acceleration_s acc{};
		acc.timestamp = hrt_absolute_time();
		acc.xyz[2] = _accel_z;
		_acc_pub.publish(acc);

		vehicle_local_position_s p{};
		p.timestamp = hrt_absolute_time();
		p.v_z_valid = true;
		p.z_valid = true;
		p.vz = _vz;
		p.z_deriv = _vz;
		p.dist_bottom_sensor_bitfield = _range_sensor ? vehicle_local_position_s::DIST_BOTTOM_SENSOR_RANGE : 0;
		p.dist_bottom_valid = _dist_valid;
		p.dist_bottom = _dist;
		_lpos_pub.publish(p);
	}

	void publishControlMode()
	{
		vehicle_control_mode_s m{};
		m.timestamp = hrt_absolute_time();
		m.flag_control_climb_rate_enabled = false;
		_mode_pub.publish(m);
	}

	void pubTakeoff(uint8_t state)
	{
		takeoff_status_s t{};
		t.timestamp = hrt_absolute_time();
		t.takeoff_state = state;
		_takeoff_pub.publish(t);
	}

	// poll vehicle_land_detected (feeding inputs every 20 ms) until pred holds; returns false on timeout
	bool waitFor(const std::function<bool(const vehicle_land_detected_s &)> &pred, std::chrono::milliseconds timeout,
		     vehicle_land_detected_s *out = nullptr)
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		vehicle_land_detected_s ld{};

		while (std::chrono::steady_clock::now() < deadline) {
			publishInputs();

			if (_land_sub.copy(&ld) && ld.timestamp > _t_start && pred(ld)) {
				if (out) { *out = ld; }

				return true;
			}

			std::this_thread::sleep_for(20ms);
		}

		return false;
	}

	// keep feeding inputs for a fixed (lower-bound) duration
	void feedFor(std::chrono::milliseconds d)
	{
		const auto end = std::chrono::steady_clock::now() + d;

		while (std::chrono::steady_clock::now() < end) {
			publishInputs();
			std::this_thread::sleep_for(20ms);
		}
	}

	// stop the instance and let the work queue drain before it is destroyed
	static void stop(RunnableMcLandDetector &ld)
	{
		ld.request_stop();
		std::this_thread::sleep_for(250ms);
	}

	static int32_t getInt(const char *n) { int32_t v = -1; param_get(param_find(n), &v); return v; }

	hrt_abstime _t_start{0};
	bool _armed{false};
	float _throttle{0.05f};
	float _vz{0.f};
	bool _range_sensor{false};
	bool _dist_valid{false};
	float _dist{0.f};
	float _accel_z{-9.81f};

	uORB::Publication<actuator_armed_s> _armed_pub{ORB_ID(actuator_armed)};
	uORB::Publication<vehicle_thrust_setpoint_s> _thrust_pub{ORB_ID(vehicle_thrust_setpoint)};
	uORB::Publication<vehicle_acceleration_s> _acc_pub{ORB_ID(vehicle_acceleration)};
	uORB::Publication<vehicle_local_position_s> _lpos_pub{ORB_ID(vehicle_local_position)};
	uORB::Publication<vehicle_control_mode_s> _mode_pub{ORB_ID(vehicle_control_mode)};
	uORB::Publication<takeoff_status_s> _takeoff_pub{ORB_ID(takeoff_status)};
	uORB::Publication<sensor_selection_s> _sel_pub{ORB_ID(sensor_selection)};
	uORB::Publication<vehicle_angular_velocity_s> _gyro_pub{ORB_ID(vehicle_angular_velocity)};
	uORB::Subscription _land_sub{ORB_ID(vehicle_land_detected)};
};

// SQE-LD-01 | LD-D01 T(2nd operand: first cycle) · LD-D02 T · LD-D03 T · LD-D09 T(1st operand: never published)
// Given: disarmed vehicle at rest, gravity on the accelerometer
// When : the detector is started on the work queue
// Then : a first vehicle_land_detected is published promptly: landed, maybe_landed and ground_contact true (disarmed),
//        no free fall, at rest
TEST_F(SqeLandDetectorRunTest, LD01_Start_FirstCyclePublishesLandedWhileDisarmed)
{
	const hrt_abstime t_start = hrt_absolute_time();
	RunnableMcLandDetector ld;
	ld.start();

	vehicle_land_detected_s out{};
	ASSERT_TRUE(waitFor([&](const vehicle_land_detected_s & m) { return m.timestamp > t_start; }, 2000ms, &out));
	EXPECT_TRUE(out.landed);
	EXPECT_TRUE(out.maybe_landed);
	EXPECT_TRUE(out.ground_contact);
	EXPECT_FALSE(out.freefall);
	EXPECT_TRUE(out.at_rest);

	stop(ld);
}

// SQE-LD-02 | LD-D09 all operands F (no change within 1 s: nothing published) then T(1st operand: 1 s elapsed)
// Given: a started detector whose outputs do not change
// When : observing the topic for 0.6 s after the first message, then until a further message arrives
// Then : no new message within 0.6 s (lower bound); the next message is the periodic 1 Hz republish, at least 1 s after
//        the first one
TEST_F(SqeLandDetectorRunTest, LD02_UnchangedOutputs_RepublishedOnlyAtOneHertz)
{
	RunnableMcLandDetector ld;
	ld.start();
	vehicle_land_detected_s first{};
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s &) { return true; }, 2000ms, &first));

	feedFor(600ms);
	vehicle_land_detected_s now{};
	ASSERT_TRUE(_land_sub.copy(&now));
	EXPECT_EQ(now.timestamp, first.timestamp);

	vehicle_land_detected_s second{};
	ASSERT_TRUE(waitFor([&](const vehicle_land_detected_s & m) { return m.timestamp != first.timestamp; }, 3000ms, &second));
	EXPECT_GE(second.timestamp - first.timestamp, 1_s);

	stop(ld);
}

// SQE-LD-03 | LD-D10 T then F(3rd operand: second take-off) · LD-D11 F(armed) then T (disarm after flight) ·
//             LD-D09 T(2nd operand: landed changed) and T(4th operand: maybe_landed changed first while landing)
// Given: armed vehicle, high throttle and climbing -> take off; then low throttle and still -> lands (still armed);
//        then takes off again; then disarms
// When : the sequence is fed to the running detector
// Then : take-off and landing are both published; on disarm the flight time is accumulated into LND_FLIGHT_T_LO:
//        at least the time we kept it flying (>= 0.8 s) and no more than the whole test duration
TEST_F(SqeLandDetectorRunTest, LD03_TakeoffLandTakeoffDisarm_RecordsFlightTime)
{
	const auto test_start = std::chrono::steady_clock::now();
	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed; }, 2000ms));

	_armed = true;
	_throttle = 0.6f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed; }, 3000ms));
	feedFor(400ms);

	_throttle = 0.05f;
	_vz = 0.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed; }, 4000ms));

	_throttle = 0.6f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed; }, 3000ms));
	feedFor(400ms);

	_armed = false;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed; }, 3000ms));
	feedFor(200ms);

	const auto test_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
			     test_start).count();
	const uint32_t flight_us = static_cast<uint32_t>(getInt("LND_FLIGHT_T_LO"));
	EXPECT_GE(flight_us, 800000u);
	EXPECT_LE(static_cast<int64_t>(flight_us), test_us);
	EXPECT_EQ(getInt("LND_FLIGHT_T_HI"), 0);

	stop(ld);
}

// SQE-LD-04 | LD-D09 T(3rd operand: only freefall changed) · LD-D10 F(2nd operand: already airborne) · LD-D03 T
// Given: armed and airborne (landed false)
// When : the accelerometer reads 0.5 m/s^2 (free fall) for longer than the 300 ms free-fall hysteresis
// Then : freefall is published true while landed stays false
TEST_F(SqeLandDetectorRunTest, LD04_Airborne_FreefallPublished)
{
	RunnableMcLandDetector ld;
	ld.start();
	_armed = true;
	_throttle = 0.6f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed; }, 3000ms));

	_accel_z = -0.5f;
	vehicle_land_detected_s out{};
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.freefall; }, 3000ms, &out));
	EXPECT_FALSE(out.landed);

	stop(ld);
}

// SQE-LD-05 | LD-D09 T(6th operand: only in_ground_effect changed)
// Given: disarmed, landed, no ground effect published yet
// When : the takeoff state becomes RAMPUP
// Then : in_ground_effect is published true; landed remains true
TEST_F(SqeLandDetectorRunTest, LD05_RampUp_GroundEffectChangePublished)
{
	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed && !m.in_ground_effect; }, 2000ms));

	pubTakeoff(takeoff_status_s::TAKEOFF_STATE_RAMPUP);
	vehicle_land_detected_s out{};
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.in_ground_effect; }, 2000ms, &out));
	EXPECT_TRUE(out.landed);

	stop(ld);
}

// SQE-LD-06 | LD-D13 T · LD-D14 T · LD-D16 T(id≠0 ∧ match) · LD-D18 T/F · LD-D19 T(1st operand) and T(2nd operand) ·
//             LD-D20 F then T · LD-D09 T(7th operand: only at_rest changed)
// Given: landed and at rest; the gyro of IMU instance 0 has device id 1234
// When : sensor_selection selects gyro 1234 and instance 0 reports gyro vibration 0.05 (> 0.02); later, after the
//        vehicle has been quiet for > 1 s, accel vibration 2.0 (> 1.2) with no gyro vibration
// Then : at_rest is published false after each vibration and true again after > 1 s of quiet
TEST_F(SqeLandDetectorRunTest, LD06_ImuVibration_ClearsAtRest)
{
	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed && m.at_rest; }, 2000ms));

	vehicle_imu_status_s imu{};
	imu.timestamp = hrt_absolute_time();
	imu.gyro_device_id = 1234;
	imu.gyro_vibration_metric = 0.05f;
	g_imu0->publish(imu);

	sensor_selection_s sel{};
	sel.timestamp = hrt_absolute_time();
	sel.gyro_device_id = 1234;
	_sel_pub.publish(sel);
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.at_rest; }, 2000ms));

	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.at_rest; }, 3000ms));

	_sel_pub.publish(sel); // same gyro id again: no instance search
	imu.timestamp = hrt_absolute_time();
	imu.gyro_vibration_metric = 0.f;
	imu.accel_vibration_metric = 2.f;
	g_imu0->publish(imu);
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.at_rest; }, 2000ms));

	imu.timestamp = hrt_absolute_time();
	imu.accel_vibration_metric = 0.1f;
	g_imu0->publish(imu);
	EXPECT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.at_rest; }, 3000ms));

	stop(ld);
}

// SQE-LD-07 | LD-D04 T then F (body rate above / below 3 deg/s) · LD-D17 T (selected gyro has no IMU status)
// Given: landed and at rest; sensor_selection names gyro 9999, which no IMU status instance reports
// When : a body rate of 0.2 rad/s (11.5 deg/s) is published, then 0.01 rad/s
// Then : the unknown gyro is tolerated (no crash, still at rest until the rotation); the fast rotation clears at_rest,
//        and after > 1 s of slow rotation at_rest is published true again
TEST_F(SqeLandDetectorRunTest, LD07_BodyRateMotion_ClearsAtRest)
{
	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed && m.at_rest; }, 2000ms));

	sensor_selection_s sel{};
	sel.timestamp = hrt_absolute_time();
	sel.gyro_device_id = 9999;
	_sel_pub.publish(sel);
	feedFor(200ms);

	vehicle_angular_velocity_s w{};
	w.timestamp = w.timestamp_sample = hrt_absolute_time();
	w.xyz[0] = 0.2f;
	_gyro_pub.publish(w);
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.at_rest; }, 2000ms));

	w.timestamp = w.timestamp_sample = hrt_absolute_time();
	w.xyz[0] = 0.01f;
	_gyro_pub.publish(w);
	EXPECT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.at_rest; }, 3000ms));

	stop(ld);
}

// SQE-LD-08 | LD-D05 T then F (range sensor makes distance observable once) · LD-D06 T (observable but invalid ->
//             hysteresis factor 3)
// Given: local position reports a range sensor (DIST_BOTTOM_SENSOR_RANGE) whose distance is currently invalid;
//        LNDMC_TRIG_TIME 0.3 s -> each landing stage 0.1 s x 3 = 0.3 s
// When : an armed, airborne vehicle switches to landing conditions (low throttle, still)
// Then : landing is NOT reported within the first 0.6 s (factor-3 chain: ground contact 0.3 + maybe 0.3 + landed 0.3 s;
//        with factor 1 the whole chain would take 0.3 s) — lower-bound check — and is reported within 5 s
TEST_F(SqeLandDetectorRunTest, LD08_RangeSensorInvalid_TriplesLandingDelay)
{
	_range_sensor = true;
	RunnableMcLandDetector ld;
	ld.start();
	_armed = true;
	_throttle = 0.6f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed; }, 3000ms));

	_throttle = 0.05f;
	_vz = 0.f;
	const auto t0 = std::chrono::steady_clock::now();
	feedFor(600ms);
	vehicle_land_detected_s mid{};
	ASSERT_TRUE(_land_sub.copy(&mid));
	EXPECT_FALSE(mid.landed);

	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed; }, 5000ms));
	EXPECT_GE(std::chrono::steady_clock::now() - t0, 600ms);

	stop(ld);
}

// SQE-LD-09 | LD-D01 T(1st operand: parameter_update received after the 1 s interval)
// Given: armed and airborne with throttle 0.3; MPC_THR_HOVER 0.5 -> low-throttle limit 0.234 (0.3 is not low)
// When : MPC_THR_HOVER is changed to 0.8 (param_set publishes parameter_update) and the detector keeps running
// Then : the detector re-reads its parameters: the limit becomes 0.12 + 0.68*0.3 = 0.324 and has_low_throttle is
//        published true (within the 1 s parameter-poll interval plus the 1 Hz republish)
TEST_F(SqeLandDetectorRunTest, LD09_ParameterUpdate_ReReadAtRuntime)
{
	RunnableMcLandDetector ld;
	ld.start();
	_armed = true;
	_throttle = 0.3f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed && !m.has_low_throttle; }, 3000ms));
	feedFor(1100ms);

	setFloat("MPC_THR_HOVER", 0.8f);
	EXPECT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.has_low_throttle; }, 4000ms));

	stop(ld);
}

// SQE-LD-10 | LD-D12 T (should_exit -> schedule cleared, module cleanup)
// Given: a running detector that republishes at least once per second
// When : request_stop() is called
// Then : after the stop no further vehicle_land_detected is published for 1.5 s (longer than the 1 Hz republish
//        period), i.e. the work item left the schedule
TEST_F(SqeLandDetectorRunTest, LD10_RequestStop_StopsPublishing)
{
	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s &) { return true; }, 2000ms));

	ld.request_stop();
	std::this_thread::sleep_for(300ms);
	vehicle_land_detected_s before{};
	ASSERT_TRUE(_land_sub.copy(&before));

	std::this_thread::sleep_for(1500ms);
	vehicle_land_detected_s after{};
	ASSERT_TRUE(_land_sub.copy(&after));
	EXPECT_EQ(after.timestamp, before.timestamp);
}

// SQE-LD-11 | LD-D06 T∧F (range sensor observable AND distance valid -> factor 1) · MLD-D27 F · MLD-D28 T/F (close-to-
//             ground check enforced) — integration of Run() with the multicopter decisions
// Given: armed, airborne; a valid range-sensor distance is available, so the close-to-ground check is enforced
// When : landing conditions (low throttle, still) at 1.5 m above ground, then at 0.5 m
// Then : at 1.5 m the vehicle is NOT reported landed even after 1.5 s (5x the normal 0.3 s landing chain: lower bound);
//        at 0.5 m (< 1.0 m) it lands
TEST_F(SqeLandDetectorRunTest, LD11_ValidRangeDistance_LandingRequiresCloseToGround)
{
	_range_sensor = true;
	_dist_valid = true;
	_dist = 5.f;
	RunnableMcLandDetector ld;
	ld.start();
	_armed = true;
	_throttle = 0.6f;
	_vz = -1.f;
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.landed; }, 3000ms));

	_throttle = 0.05f;
	_vz = 0.f;
	_dist = 1.5f;
	feedFor(1500ms);
	vehicle_land_detected_s high{};
	ASSERT_TRUE(_land_sub.copy(&high));
	EXPECT_FALSE(high.landed);
	EXPECT_FALSE(high.close_to_ground_or_skipped_check);

	_dist = 0.5f;
	EXPECT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed; }, 4000ms));

	stop(ld);
}

// SQE-LD-12 | LD-D16 F(2nd operand: instance 0 has a non-zero but different gyro id) then T(instance 1) · LD-D18 T
// Given: landed, at rest; IMU instance 0 reports gyro 1111, instance 1 reports gyro 2222 (both quiet)
// When : sensor_selection selects gyro 2222; afterwards instance 0 (the NON-selected gyro) vibrates continuously for
//        1.5 s; finally gyro 1111 is selected and instance 0 vibrates again
// Then : vibration of the non-selected IMU is ignored (at_rest is never published false during the 1.5 s); once 1111
//        is selected the same vibration clears at_rest (positive control)
TEST_F(SqeLandDetectorRunTest, LD12_AtRest_FollowsOnlySelectedGyroImu)
{
	vehicle_imu_status_s imu0{}, imu1{};
	imu0.timestamp = imu1.timestamp = hrt_absolute_time();
	imu0.gyro_device_id = 1111;
	imu1.gyro_device_id = 2222;
	g_imu0->publish(imu0);
	g_imu1->publish(imu1);

	RunnableMcLandDetector ld;
	ld.start();
	ASSERT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return m.landed && m.at_rest; }, 2000ms));

	sensor_selection_s sel{};
	sel.timestamp = hrt_absolute_time();
	sel.gyro_device_id = 2222;
	_sel_pub.publish(sel);
	feedFor(300ms);

	// the non-selected IMU vibrates continuously for 1.5 s; at_rest must never be published false
	imu0.gyro_vibration_metric = 0.05f;
	bool ever_moving = false;
	const auto end = std::chrono::steady_clock::now() + 1500ms;

	while (std::chrono::steady_clock::now() < end) {
		imu0.timestamp = hrt_absolute_time();
		g_imu0->publish(imu0);
		publishInputs();
		vehicle_land_detected_s m{};

		if (_land_sub.copy(&m) && m.timestamp > _t_start && !m.at_rest) { ever_moving = true; }

		std::this_thread::sleep_for(20ms);
	}

	EXPECT_FALSE(ever_moving);

	sel.timestamp = hrt_absolute_time();
	sel.gyro_device_id = 1111;
	_sel_pub.publish(sel);
	feedFor(300ms);
	imu0.timestamp = hrt_absolute_time();
	g_imu0->publish(imu0);
	EXPECT_TRUE(waitFor([](const vehicle_land_detected_s & m) { return !m.at_rest; }, 2000ms));

	stop(ld);
}
