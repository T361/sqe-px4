/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : LandDetector.cpp — Run(), first cycle (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional with PX4's real work-queue manager (see SqeLandDetectorRunTest.cpp)
 * Why a separate binary: the first-cycle condition `_parameter_update_sub.updated() || _land_detected.timestamp == 0`
 *                      only reaches its second operand when no parameter_update has ever been published in the process.
 *                      Every param_set()/param_reset_all() publishes one, so this binary uses only
 *                      param_set_no_notification() and contains a single test.
 * Decisions covered : LD-D01 F∨T (second operand: never published yet)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <drivers/drv_hrt.h>
#include <hrt_work.h>
#include <parameters/param.h>
#include <px4_platform_common/px4_work_queue/WorkQueueManager.hpp>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/actuator_armed.h>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/vehicle_acceleration.h>
#include <uORB/topics/vehicle_land_detected.h>
#include <uORB/topics/vehicle_local_position.h>
#include <uORB/topics/vehicle_thrust_setpoint.h>

#include "MulticopterLandDetector.h"

using namespace std::chrono_literals;

class RunnableMcLandDetector : public land_detector::MulticopterLandDetector
{
public:
	using land_detector::LandDetector::request_stop;
};

// SQE-LDB-01 | LD-D01 F∨T (no parameter_update yet: the first cycle loads parameters through timestamp == 0)
// Given: no parameter_update has been published in this process (precondition checked); MPC_THR_HOVER 0.8 written
//        without notification; armed, airborne, throttle 0.3
// When : the detector is started and runs its first cycle
// Then : the first published message already uses the configured hover 0.8: low-throttle limit
//        0.12 + (0.8-0.12)*0.3 = 0.324, so throttle 0.3 is reported as low throttle (with the default hover 0.5 the
//        limit would be 0.234 and 0.3 would not be low)
TEST(SqeLandDetectorBootTest, LDB01_FirstCycleWithoutParameterUpdate_LoadsParameters)
{
	ASSERT_NE(orb_exists(ORB_ID(parameter_update), 0), PX4_OK) << "precondition: no parameter_update published yet";

	hrt_work_queue_init();
	hrt_init();
	ASSERT_EQ(px4::WorkQueueManagerStart(), PX4_OK);

	const float hover = 0.8f, thr_min = 0.12f, trig = 0.3f;
	const int32_t use_hte = 0;
	ASSERT_EQ(param_set_no_notification(param_find("MPC_THR_HOVER"), &hover), PX4_OK);
	ASSERT_EQ(param_set_no_notification(param_find("MPC_THR_MIN"), &thr_min), PX4_OK);
	ASSERT_EQ(param_set_no_notification(param_find("MPC_USE_HTE"), &use_hte), PX4_OK);
	ASSERT_EQ(param_set_no_notification(param_find("LNDMC_TRIG_TIME"), &trig), PX4_OK);

	uORB::Publication<actuator_armed_s> armed_pub{ORB_ID(actuator_armed)};
	uORB::Publication<vehicle_thrust_setpoint_s> thrust_pub{ORB_ID(vehicle_thrust_setpoint)};
	uORB::Publication<vehicle_acceleration_s> acc_pub{ORB_ID(vehicle_acceleration)};
	uORB::Publication<vehicle_local_position_s> lpos_pub{ORB_ID(vehicle_local_position)};
	uORB::Subscription land_sub{ORB_ID(vehicle_land_detected)};

	auto feed = [&]() {
		actuator_armed_s a{};
		a.timestamp = hrt_absolute_time();
		a.armed = true;
		armed_pub.publish(a);
		vehicle_thrust_setpoint_s t{};
		t.timestamp = hrt_absolute_time();
		t.xyz[2] = -0.3f;
		thrust_pub.publish(t);
		vehicle_acceleration_s acc{};
		acc.timestamp = hrt_absolute_time();
		acc.xyz[2] = -9.81f;
		acc_pub.publish(acc);
		vehicle_local_position_s p{};
		p.timestamp = hrt_absolute_time();
		p.v_z_valid = true;
		p.vz = -1.f;
		lpos_pub.publish(p);
	};

	feed();
	RunnableMcLandDetector ld;
	ld.start();

	vehicle_land_detected_s out{};
	bool got = false;

	for (int i = 0; i < 100 && !got; i++) {
		feed();
		std::this_thread::sleep_for(20ms);
		got = land_sub.copy(&out) && out.timestamp != 0;
	}

	ASSERT_TRUE(got);
	EXPECT_TRUE(out.has_low_throttle);

	ld.request_stop();
	std::this_thread::sleep_for(250ms);
}
