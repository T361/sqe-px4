/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : FailureDetector.cpp / FailureDetector.hpp — updateImbalancedPropStatus() (PX4-Autopilot
 *                      v1.17.0, d6f12ad1)
 * Test level        : GTest functional — justification: imbalanced-prop detection needs real multi-instance
 *                      vehicle_imu_status publications, which must be created once (PublicationMulti, fixed
 *                      instance order) and persist for the whole process; kept in its own binary so no other
 *                      FailureDetector test's IMU instances leak into these decisions.
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (FD-D06, FD-D20..D28, REF_05)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/PublicationMulti.hpp>
#include <uORB/topics/sensor_selection.h>
#include <uORB/topics/vehicle_imu_status.h>

#include "FailureDetector.hpp"

using namespace time_literals;

namespace
{
// Two multi-instance publishers created once for the whole test binary (SetUpTestSuite), instance 0 then 1,
// matching SPEC_02 §6's requirement that multi-instance topics be set up in a fixed order before any test runs.
uORB::PublicationMulti<vehicle_imu_status_s> *g_imu_pub0{nullptr};
uORB::PublicationMulti<vehicle_imu_status_s> *g_imu_pub1{nullptr};
}

class SqeFailureDetectorImuTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		g_imu_pub0 = new uORB::PublicationMulti<vehicle_imu_status_s>(ORB_ID(vehicle_imu_status));
		vehicle_imu_status_s seed0{};
		seed0.timestamp = hrt_absolute_time();
		seed0.accel_device_id = 0; // neutral seed, instance 0 reserved
		g_imu_pub0->publish(seed0);

		g_imu_pub1 = new uORB::PublicationMulti<vehicle_imu_status_s>(ORB_ID(vehicle_imu_status));
		vehicle_imu_status_s seed1{};
		seed1.timestamp = hrt_absolute_time();
		seed1.accel_device_id = 0; // neutral seed, instance 1 reserved
		g_imu_pub1->publish(seed1);
	}

	static void TearDownTestSuite()
	{
		delete g_imu_pub0;
		g_imu_pub0 = nullptr;
		delete g_imu_pub1;
		g_imu_pub1 = nullptr;
	}

	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 1_s);

		_status = {};
		_status.arming_state = vehicle_status_s::ARMING_STATE_ARMED;
		_status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;

		_mode = {};
		_mode.flag_control_attitude_enabled = false; // isolate: no attitude subsystem in these tests
		setParamInt("FD_ESCS_EN", 0);
		setParamInt("FD_ACT_EN", 0);
		setParamInt("FD_EXT_ATS_EN", 0);
	}

	static void setParamInt(const char *name, int32_t v)
	{
		ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name;
	}
	static void setParamFloat(const char *name, float v)
	{
		ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name;
	}

	void publishSelection(uint32_t accel_device_id)
	{
		sensor_selection_s sel{};
		sel.timestamp = hrt_absolute_time();
		sel.accel_device_id = accel_device_id;
		_sel_pub.publish(sel);
	}

	void publishImu0(uint32_t accel_device_id, float var_x, float var_y, float var_z)
	{
		vehicle_imu_status_s imu{};
		imu.timestamp = hrt_absolute_time();
		imu.accel_device_id = accel_device_id;
		imu.var_accel[0] = var_x;
		imu.var_accel[1] = var_y;
		imu.var_accel[2] = var_z;
		g_imu_pub0->publish(imu);
	}

	void publishImu1(uint32_t accel_device_id, float var_x, float var_y, float var_z)
	{
		vehicle_imu_status_s imu{};
		imu.timestamp = hrt_absolute_time();
		imu.accel_device_id = accel_device_id;
		imu.var_accel[0] = var_x;
		imu.var_accel[1] = var_y;
		imu.var_accel[2] = var_z;
		g_imu_pub1->publish(imu);
	}

	uORB::Publication<sensor_selection_s> _sel_pub{ORB_ID(sensor_selection)};

	vehicle_status_s _status{};
	vehicle_control_mode_s _mode{};
};

// SQE-FDI-01 | FD-D06T FD-D20T FD-D21T FD-D22F FD-D26T FD-D27T FD-D28 (T∧T)
// Given: FD_IMB_PROP_THR 30; sensor_selection id 1111 published; instance 0 publishes id 1111 with
//        var_accel {40000,40000,0} and a timestamp 10s after epoch (first sample, so dt clamps to 1.0s,
//        alpha = dt/(tau+dt) = 1/6 with the 5s time-constant)
// When : update()
// Then : imbalanced_prop flag true; getImbalancedPropMetric() ~= 200/6 ~= 33.33 — std_x=std_y=200 (sqrt(40000)),
//        std_z=0, metric=(200+200)/2-0=200, filtered = 0 + alpha*(200-0) = 33.33 (independent arithmetic, not
//        copied from the production expression)
TEST_F(SqeFailureDetectorImuTest, FDI01_ImbalancedVariance_SetsFlagWithExpectedMetric)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(1111);
	publishImu0(1111, 40000.f, 40000.f, 0.f);
	publishImu1(0, 0.f, 0.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_NEAR(fd.getImbalancedPropMetric(), 33.3333f, 0.05f);
}

// SQE-FDI-02 | FD-D28 (T∧T), threshold comparison False
// Given: FD_IMB_PROP_THR 30; selection id 2222; instance 0 publishes id 2222 with equal variances {100,100,100}
// When : update()
// Then : imbalanced_prop flag false — std_x=std_y=std_z=10, metric=(10+10)/2-10=0, well under the 30 threshold
TEST_F(SqeFailureDetectorImuTest, FDI02_EqualVariances_MetricZero_FlagStaysFalse)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(2222);
	publishImu0(2222, 100.f, 100.f, 100.f);
	publishImu1(0, 0.f, 0.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_NEAR(fd.getImbalancedPropMetric(), 0.f, 1e-3f);
}

// SQE-FDI-03 | FD-D06F
// Given: FD_IMB_PROP_THR 0 (disables the whole subroutine: updateImbalancedPropStatus() is never called)
// When : update() with a selection + IMU publish that would otherwise clearly exceed any threshold
// Then : the metric stays at its default-constructed 0 (AlphaFilter never updated) and the flag stays false,
//        because FD-D06 is False and the subroutine body never runs
TEST_F(SqeFailureDetectorImuTest, FDI03_ThresholdZero_SubroutineNeverRuns)
{
	setParamInt("FD_IMB_PROP_THR", 0);
	FailureDetector fd{nullptr};

	publishSelection(3333);
	publishImu0(3333, 40000.f, 40000.f, 0.f);
	publishImu1(0, 0.f, 0.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_FLOAT_EQ(fd.getImbalancedPropMetric(), 0.f);
}

// SQE-FDI-04 | FD-D22T FD-D23 FD-D24F FD-D25 (F then T)
// Given: FD_IMB_PROP_THR 30; selection id 2222; instance 0 publishes an unrelated id 1111 (small variance);
//        instance 1 publishes id 2222 (the selected id) with a large imbalance
// When : update()
// Then : the detector's instance-search loop (FD-D22 True because the just-copied imu_status.accel_device_id
//        (1111, from the default/last-copied instance) != selected (2222)) scans instances until ChangeInstance
//        lands on instance 1, whose id matches -> detected through instance 1 (imbalanced_prop true)
TEST_F(SqeFailureDetectorImuTest, FDI04_SelectedAccelOnInstance1_DetectedViaInstanceSearch)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(2222);
	publishImu0(1111, 100.f, 100.f, 100.f);
	publishImu1(2222, 40000.f, 40000.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_TRUE(fd.getStatusFlags().imbalanced_prop);
}

// SQE-FDI-05 | FD-D24T FD-D28 (2nd operand False)
// Given: FD_IMB_PROP_THR 30; selection id 3333, which is published on neither instance 0 nor instance 1
// When : update()
// Then : the instance search scans all ORB_MULTI_MAX_INSTANCES(10) instances; ChangeInstance() returns false for
//        the non-existing instances 2..9 (FD-D24 True, continue), and neither instance 0 nor 1 match id 3333, so
//        no update to imbalanced_prop/metric happens this call — the flag stays at its default-constructed false
TEST_F(SqeFailureDetectorImuTest, FDI05_SelectedAccelOnNoInstance_NoDetection)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(3333);
	publishImu0(1111, 40000.f, 40000.f, 0.f);
	publishImu1(2222, 40000.f, 40000.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_FLOAT_EQ(fd.getImbalancedPropMetric(), 0.f);
}

// SQE-FDI-06 | FD-D22F FD-D28 (1st operand False — device id 0)
// Given: FD_IMB_PROP_THR 30; selection accel_device_id 0 (never set, i.e. "no sensor selected" sentinel);
//        instance 0 also publishes accel_device_id 0
// When : update()
// Then : imbalanced_prop stays false — FD-D28's first operand (imu_status.accel_device_id != 0) is False, so the
//        metric computation is skipped even though the ids match
TEST_F(SqeFailureDetectorImuTest, FDI06_DeviceIdZero_NoComputation)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(0);
	publishImu0(0, 40000.f, 40000.f, 0.f);
	publishImu1(0, 0.f, 0.f, 0.f);
	fd.update(_status, _mode);

	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_FLOAT_EQ(fd.getImbalancedPropMetric(), 0.f);
}

// SQE-FDI-07 | FD-D20F FD-D26F
// Given: FD_IMB_PROP_THR 30; selection + IMU instance 0 published once and consumed by an initial update() that
//        establishes a nonzero metric
// When : a second update() is called without publishing any new sensor_selection or vehicle_imu_status message
// Then : the metric is unchanged (FD-D20/FD-D26 are both False — neither subscriber saw a new message, so the
//        AlphaFilter is never re-updated)
TEST_F(SqeFailureDetectorImuTest, FDI07_NoNewImuOrSelection_MetricUnchanged)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	publishSelection(1111);
	publishImu0(1111, 40000.f, 40000.f, 0.f);
	publishImu1(0, 0.f, 0.f, 0.f);
	fd.update(_status, _mode);
	const float metric_before = fd.getImbalancedPropMetric();
	ASSERT_GT(metric_before, 0.f);

	fd.update(_status, _mode);

	EXPECT_FLOAT_EQ(fd.getImbalancedPropMetric(), metric_before);
}

// SQE-FDI-08 | FD-D25F (1st operand — copy(&imu_status) False — P10 gap closure: L212's branch had never been
//            exercised; instances 0/1 are always publish()-ed before use by this fixture's SetUpTestSuite(), so
//            copy() always succeeded on them. DataValidator::copy() can only return false when the uORB device
//            node has been advertised (so ChangeInstance() succeeds — orb_device_node_exists() checks only
//            advertisement) but never published (so DeviceNode::_data stays nullptr — allocated lazily inside
//            DeviceNode::write(), only reached from publish()/orb_publish(), confirmed by reading
//            uORBDeviceNode.cpp L60-90 and L140-176). A third PublicationMulti instance that calls advertise()
//            but never publish() reproduces exactly that state on instance 2 (the next free slot after the
//            fixture's 0 and 1).
// Given: FD_IMB_PROP_THR 30; instance 2's vehicle_imu_status node is advertised (via advertise(), landing on
//        instance 2 since 0/1 are already taken by the fixture) but never published to; selection id 9999, which
//        matches neither instance 0 nor instance 1's published ids
// When : update() — the instance-search loop reaches i=2: ChangeInstance(2) succeeds (the node exists), but
//        copy(&imu_status) returns false (no data ever written) -> FD-D25's first operand is False -> the `if`
//        body (L213-216, including the `break`) is skipped -> the loop continues to i=3..9 (ChangeInstance fails,
//        FD-D24 True) -> loop ends with no match found
// Then : no crash (copy() returning false on a never-published-but-advertised instance is handled, not
//        dereferenced); imbalanced_prop stays false, matching the "no match found" outcome already characterized
//        by FDI05 for the "never advertised at all" case — this test is the companion "advertised but empty" case
TEST_F(SqeFailureDetectorImuTest, FDI08_InstanceAdvertisedButNeverPublished_CopyFailsSearchContinues)
{
	setParamInt("FD_IMB_PROP_THR", 30);
	FailureDetector fd{nullptr};

	uORB::PublicationMulti<vehicle_imu_status_s> imu_pub2{ORB_ID(vehicle_imu_status)};
	ASSERT_TRUE(imu_pub2.advertise()) << "instance 2 must be advertised (node exists) but never published";

	publishSelection(9999);
	publishImu0(1111, 100.f, 100.f, 100.f);
	publishImu1(2222, 40000.f, 40000.f, 0.f);

	EXPECT_NO_FATAL_FAILURE(fd.update(_status, _mode));

	EXPECT_FALSE(fd.getStatusFlags().imbalanced_prop);
	EXPECT_FLOAT_EQ(fd.getImbalancedPropMetric(), 0.f);
}
