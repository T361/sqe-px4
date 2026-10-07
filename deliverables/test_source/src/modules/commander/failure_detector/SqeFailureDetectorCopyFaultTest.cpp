/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : FailureDetector.cpp — updateImbalancedPropStatus() (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional (multi-instance vehicle_imu_status publications, parameters)
 * Test double       : a link-time fake of uORB::Manager::orb_data_copy. This binary is linked with
 *                      -Wl,--wrap=_ZN4uORB7Manager13orb_data_copyEPvS1_Rjb, so the copy() calls made by
 *                      FailureDetector's uORB::Subscription objects reach __wrap_... below. The fake forwards to the
 *                      real uORB unless a test arms a one-shot failure for one topic. This drives FD-D21
 *                      (_sensor_selection_sub.copy() after updated()) False — previously gap G-07, unreachable with the
 *                      real uORB because a copy right after a successful updated() cannot fail without a concurrent
 *                      writer/unadvertise.
 * Decisions covered : FD-D21 F (and T as positive control), FD-D20 T
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/PublicationMulti.hpp>
#include <uORB/topics/sensor_selection.h>
#include <uORB/topics/vehicle_imu_status.h>
#include <uORB/uORBDeviceNode.hpp>

#include "FailureDetector.hpp"

using namespace time_literals;

namespace
{
const orb_metadata *g_copy_fail_topic{nullptr};
int g_copy_fail_hits{0};

uORB::PublicationMulti<vehicle_imu_status_s> *g_imu0{nullptr};
uORB::PublicationMulti<vehicle_imu_status_s> *g_imu1{nullptr};
}

extern "C" bool __real__ZN4uORB7Manager13orb_data_copyEPvS1_Rjb(void *node, void *dst, unsigned &gen, bool only_if_updated);
extern "C" bool __wrap__ZN4uORB7Manager13orb_data_copyEPvS1_Rjb(void *node, void *dst, unsigned &gen, bool only_if_updated)
{
	if (g_copy_fail_topic != nullptr && node != nullptr
	    && static_cast<uORB::DeviceNode *>(node)->get_meta() == g_copy_fail_topic) {
		g_copy_fail_topic = nullptr;
		g_copy_fail_hits++;
		return false;
	}

	return __real__ZN4uORB7Manager13orb_data_copyEPvS1_Rjb(node, dst, gen, only_if_updated);
}

class SqeFailureDetectorCopyFaultTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		// fixed instance order: 0 then 1, created once for the whole binary
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
	}

	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 1_s);
		g_copy_fail_topic = nullptr;
		g_copy_fail_hits = 0;

		_status = {};
		_status.arming_state = vehicle_status_s::ARMING_STATE_ARMED;
		_status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;
		_mode = {};
		setInt("FD_ESCS_EN", 0);
		setInt("FD_ACT_EN", 0);
		setInt("FD_EXT_ATS_EN", 0);
		setInt("FD_IMB_PROP_THR", 30);
	}

	static void setInt(const char *name, int32_t v) { ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name; }

	void publishSelection(uint32_t accel_device_id)
	{
		sensor_selection_s sel{};
		sel.timestamp = hrt_absolute_time();
		sel.accel_device_id = accel_device_id;
		_sel_pub.publish(sel);
	}

	// explicit sample timestamps (1 s apart) make every filter step use dt = 1 s, alpha = 1/(5+1) = 1/6
	static void publishImu(uORB::PublicationMulti<vehicle_imu_status_s> *pub, uint32_t id, float vx, float vy, float vz,
			       hrt_abstime ts)
	{
		vehicle_imu_status_s imu{};
		imu.timestamp = ts;
		imu.accel_device_id = id;
		imu.var_accel[0] = vx;
		imu.var_accel[1] = vy;
		imu.var_accel[2] = vz;
		pub->publish(imu);
	}

	uORB::Publication<sensor_selection_s> _sel_pub{ORB_ID(sensor_selection)};
	vehicle_status_s _status{};
	vehicle_control_mode_s _mode{};
};

// SQE-FDC-01 | FD-D20 T · FD-D21 F (copy fails after updated) then T (positive control) — closes G-07
// Given: FD_IMB_PROP_THR 30; IMU A (id 1111, instance 0) is imbalanced: var {40000,40000,0} -> metric
//        (200+200)/2 - 0 = 200; IMU B (id 2222, instance 1) is strongly balanced: var {0,0,1e6} -> metric -1000
// When : (1) select A and update: the detector follows A; (2) publish a selection of B but the copy of
//        sensor_selection fails once, then update; (3) publish the selection of B again (no fault) and update
// Then : (1) metric = 200/6 = 33.33 > 30 -> imbalanced; (2) the fake was hit once, the selected device is still A, so
//        A's sample is filtered again: 33.33 + (200-33.33)/6 = 61.11, still imbalanced; (3) the detector now follows B:
//        61.11 + (-1000-61.11)/6 = -115.74, no longer imbalanced (hand-computed first-order filter steps)
TEST_F(SqeFailureDetectorCopyFaultTest, FDC01_SelectionCopyFails_KeepsPreviousImu)
{
	FailureDetector fd{nullptr};
	const hrt_abstime t = hrt_absolute_time();

	publishSelection(1111);
	publishImu(g_imu0, 1111, 40000.f, 40000.f, 0.f, t);
	publishImu(g_imu1, 2222, 0.f, 0.f, 1e6f, t);
	fd.update(_status, _mode);
	const float m1 = 200.f / 6.f;
	EXPECT_NEAR(fd.getImbalancedPropMetric(), m1, 1e-2f);
	EXPECT_TRUE(fd.getStatusFlags().imbalanced_prop);

	publishSelection(2222);
	g_copy_fail_topic = ORB_ID(sensor_selection);
	publishImu(g_imu0, 1111, 40000.f, 40000.f, 0.f, t + 1_s);
	publishImu(g_imu1, 2222, 0.f, 0.f, 1e6f, t + 1_s);
	fd.update(_status, _mode);
	EXPECT_EQ(g_copy_fail_hits, 1);
	const float m2 = m1 + (200.f - m1) / 6.f;
	EXPECT_NEAR(fd.getImbalancedPropMetric(), m2, 1e-2f);
	EXPECT_TRUE(fd.getStatusFlags().imbalanced_prop);

	publishSelection(2222);
	publishImu(g_imu0, 1111, 40000.f, 40000.f, 0.f, t + 2_s);
	publishImu(g_imu1, 2222, 0.f, 0.f, 1e6f, t + 2_s);
	fd.update(_status, _mode);
	EXPECT_NEAR(fd.getImbalancedPropMetric(), m2 + (-1000.f - m2) / 6.f, 1e-1f);
	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
}
