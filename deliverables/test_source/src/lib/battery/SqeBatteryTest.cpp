/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : src/lib/battery/battery.cpp / battery.h (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional — justification: Battery reads BATn_* / BAT_* parameters at construction,
 *                      subscribes to vehicle_status and flight_phase_estimation and publishes battery_status, so it
 *                      needs the parameter store and uORB that gtest_functional_main initialises.
 * Test doubles      : (1) TestableBattery — a test subclass (seam) that only re-exports the protected updateParams()
 *                      and read-only views of protected state; it overrides no behaviour.
 *                      (2) A link-time fake of uORB::Manager::orb_data_copy (this binary is linked with
 *                      -Wl,--wrap=_ZN4uORB7Manager13orb_data_copyEPvS1_Rjb); it forwards every call to the real
 *                      function unless a test arms a one-shot failure for one topic, which is how the
 *                      "copy() fails right after updated()" branch (BAT-D26 False) is reached.
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (BAT-D01..BAT-D35)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include <lib/battery/battery.h>
#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/battery_status.h>
#include <uORB/topics/flight_phase_estimation.h>
#include <uORB/topics/vehicle_status.h>
#include <uORB/uORBDeviceNode.hpp>

using namespace time_literals;

// ---- link-time fake for uORB::Manager::orb_data_copy -------------------------------------------------------------
namespace
{
const orb_metadata *g_copy_fail_topic{nullptr}; // armed one-shot failure for this topic, nullptr = pass-through
int g_copy_fail_hits{0};
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

// ---- seam: re-exports protected members, no behaviour overridden -------------------------------------------------
class TestableBattery : public Battery
{
public:
	explicit TestableBattery(int index, uint8_t source = 0, int sample_interval_us = 100000) :
		Battery(index, nullptr, sample_interval_us, source) {}

	using Battery::updateParams;
	int idx() const { return _index; }
	bool firstParameterUpdate() const { return _first_parameter_update; }
};

namespace
{
constexpr hrt_abstime T0 = 100_s;
}

class SqeBatteryTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		ASSERT_GT(hrt_absolute_time(), 5_s);

		setInt("BAT1_N_CELLS", 4);
		setFloat("BAT1_V_EMPTY", 3.5f);
		setFloat("BAT1_V_CHARGED", 4.1f);
		setFloat("BAT1_R_INTERNAL", 0.01f);
		setFloat("BAT1_CAPACITY", -1.f);
		setInt("BAT1_SOURCE", 0);
		setFloat("BAT_LOW_THR", 0.15f);
		setFloat("BAT_CRIT_THR", 0.07f);
		setFloat("BAT_EMERGEN_THR", 0.05f);
		setFloat("BAT_AVRG_CURRENT", 15.f);

		// neutral, disarmed multicopter + unknown flight phase, so a fresh Battery never inherits a previous test's
		// vehicle state through uORB's latest-sample semantics
		publishStatus(false, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
		publishPhase(flight_phase_estimation_s::FLIGHT_PHASE_UNKNOWN, 1);
		g_copy_fail_topic = nullptr;
		g_copy_fail_hits = 0;
	}

	static void setInt(const char *name, int32_t v) { ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name; }
	static void setFloat(const char *name, float v) { ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name; }

	void publishStatus(bool armed, uint8_t vehicle_type)
	{
		vehicle_status_s s{};
		s.timestamp = hrt_absolute_time();
		s.arming_state = armed ? vehicle_status_s::ARMING_STATE_ARMED : vehicle_status_s::ARMING_STATE_DISARMED;
		s.vehicle_type = vehicle_type;
		_status_pub.publish(s);
	}

	void publishPhase(uint8_t phase, hrt_abstime timestamp)
	{
		flight_phase_estimation_s p{};
		p.timestamp = timestamp;
		p.flight_phase = phase;
		_phase_pub.publish(p);
	}

	// connect at `voltage` and step the battery once per second from T0 for `steps` samples
	static void stepConnected(Battery &b, float voltage, float current, int steps, hrt_abstime start = T0)
	{
		b.setConnected(true);
		b.updateVoltage(voltage);
		b.updateCurrent(current);

		for (int k = 0; k < steps; k++) {
			b.updateBatteryStatus(start + static_cast<hrt_abstime>(k) * 1_s);
		}
	}

	uORB::Publication<vehicle_status_s> _status_pub{ORB_ID(vehicle_status)};
	uORB::Publication<flight_phase_estimation_s> _phase_pub{ORB_ID(flight_phase_estimation)};
};

// SQE-BAT-01 | BAT-D01 T(index<1) T(index>9) · BAT-D02 T(both operands)
// Given: battery indices 0 and 10 (outside the documented 1..9 range)
// When : construction
// Then : both fall back to index 1 (status id 1) and therefore read BAT1_N_CELLS (4)
TEST_F(SqeBatteryTest, BAT01_IndexOutOfRange_DefaultsToOne)
{
	TestableBattery low(0);
	TestableBattery high(10);

	EXPECT_EQ(low.idx(), 1);
	EXPECT_EQ(high.idx(), 1);
	EXPECT_EQ(low.getBatteryStatus().id, 1);
	EXPECT_EQ(high.getBatteryStatus().id, 1);
	EXPECT_EQ(low.cell_count(), 4);
	EXPECT_EQ(high.cell_count(), 4);
}

// SQE-BAT-02 | BAT-D01 F · BAT-D02 F · BAT-D03 F
// Given: BAT2_N_CELLS 6, BAT2_V_CHARGED 4.2 (index 2 has its own parameter set)
// When : construction with index 2
// Then : index kept (status id 2) and the BAT2_* values are the ones read
TEST_F(SqeBatteryTest, BAT02_ValidIndex_ReadsItsOwnParameters)
{
	setInt("BAT2_N_CELLS", 6);
	setFloat("BAT2_V_CHARGED", 4.2f);
	TestableBattery b(2);

	EXPECT_EQ(b.idx(), 2);
	EXPECT_EQ(b.getBatteryStatus().id, 2);
	EXPECT_EQ(b.cell_count(), 6);
	EXPECT_FLOAT_EQ(b.full_cell_voltage(), 4.2f);
}

// SQE-BAT-03 | BAT-D03 T · BAT-D10 F · BAT-D35 2nd operand F
// Given: index 4 — the parameter set only defines BAT1..BAT3 (module.yaml num_instances 3), so every BAT4_* lookup
//        returns PARAM_INVALID
// When : construction
// Then : no BAT4 value can be read: cell count and empty/full voltages stay at their zero initial values and the
//        status' voltage-based SoC field reports the "unknown" value -1
TEST_F(SqeBatteryTest, BAT03_IndexWithoutParameters_HandlesInvalidAndCellCountZero)
{
	ASSERT_EQ(param_find("BAT4_V_EMPTY"), PARAM_INVALID);
	TestableBattery b(4);

	EXPECT_EQ(b.idx(), 4);
	EXPECT_EQ(b.cell_count(), 0);
	EXPECT_FLOAT_EQ(b.empty_cell_voltage(), 0.f);
	EXPECT_FLOAT_EQ(b.getBatteryStatus().volt_based_soc_estimate, -1.f);
}

// SQE-BAT-04 | BAT-D04 T (2.0 V < 2.1 V) then boundary F (exactly 2.1 V)
// Given: a battery the driver reports as connected
// When : updateBatteryStatus with 2.0 V, then with exactly LITHIUM_BATTERY_RECOGNITION_VOLTAGE (2.1 V) on a second
//        battery
// Then : 2.0 V overrides the driver and reports disconnected; 2.1 V is not below the threshold so "connected" is kept
TEST_F(SqeBatteryTest, BAT04_RecognitionVoltage_BelowForcesDisconnectAtThresholdKeeps)
{
	TestableBattery below(1);
	stepConnected(below, 2.0f, 0.f, 1);
	EXPECT_FALSE(below.getBatteryStatus().connected);

	TestableBattery at(1);
	stepConnected(at, 2.1f, 0.f, 1);
	EXPECT_TRUE(at.getBatteryStatus().connected);
}

// SQE-BAT-05 | BAT-D05 T(2nd operand: first sample) then F,F · BAT-D06 F then T · BAT-D09 F then T · BAT-D20 T
// Given: connected at 14.0 V = 3.5 V/cell = BAT1_V_EMPTY, i.e. state of charge 0 (an EMERGENCY level)
// When : samples at T0, T0+2 s (exactly the 2 s settling time) and T0+2 s+1 us
// Then : the warning is only evaluated once the battery is "initialised", which requires strictly more than 2 s
//        since the last unconnected sample: NONE at T0 and at T0+2 s, EMERGENCY at T0+2 s+1 us
TEST_F(SqeBatteryTest, BAT05_WarningOnlyAfterTwoSecondSettling_BoundaryStrict)
{
	TestableBattery b(1);
	b.setConnected(true);
	b.updateVoltage(14.0f);
	b.updateCurrent(0.f);

	b.updateBatteryStatus(T0);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_NONE);

	b.updateBatteryStatus(T0 + 2_s);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_NONE);

	b.updateBatteryStatus(T0 + 2_s + 1);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_EMERGENCY);
}

// SQE-BAT-06 | BAT-D05 T(1st operand: disconnected) · BAT-D09 F(not connected) · BAT-D06 F after reconnect
// Given: an initialised battery that reached EMERGENCY at 14.0 V
// When : it disconnects (one sample), reconnects at 16.4 V (full) 1 s later, then is sampled again 2.5 s later
// Then : while disconnected and during the new 2 s settling the warning is NOT re-evaluated (stays EMERGENCY even
//        though the voltage is now full); once settled again the warning becomes NONE
TEST_F(SqeBatteryTest, BAT06_Disconnect_RestartsSettlingBeforeWarningUpdates)
{
	TestableBattery b(1);
	stepConnected(b, 14.0f, 0.f, 4);
	ASSERT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_EMERGENCY);

	const hrt_abstime t_off = T0 + 10_s;
	b.setConnected(false);
	b.updateBatteryStatus(t_off);
	EXPECT_FALSE(b.getBatteryStatus().connected);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_EMERGENCY);

	b.setConnected(true);
	b.updateVoltage(16.4f);
	b.updateBatteryStatus(t_off + 1_s);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_EMERGENCY);

	b.updateBatteryStatus(t_off + 3500_ms);
	EXPECT_EQ(b.getBatteryStatus().warning, battery_status_s::WARNING_NONE);
}

// SQE-BAT-07 | BAT-D07 T(all 4) · BAT-D14 F · BAT-D15 T · BAT-D16 T · BAT-D18 T · BAT-D08 T · BAT-D19 F(capacity 0)
// Given: 4 cells, V_EMPTY 3.5, V_CHARGED 4.1, user internal resistance 0.01 Ohm/cell, capacity unknown
// When : one connected sample at 15.2 V and 10 A
// Then : the per-cell voltage is load-compensated with the user resistance: 15.2/4 + 0.01*10 = 3.9 V, so the
//        voltage-based state of charge is (3.9-3.5)/(4.1-3.5) = 0.6667 (hand-computed linear interpolation)
TEST_F(SqeBatteryTest, BAT07_UserInternalResistance_CompensatesLoadDrop)
{
	TestableBattery b(1);
	stepConnected(b, 15.2f, 10.f, 1);

	EXPECT_NEAR(b.getBatteryStatus().remaining, 0.4f / 0.6f, 1e-4f);
}

// SQE-BAT-08 | BAT-D16 F · BAT-D18 F (BAT1_R_INTERNAL -1: use the online estimate)
// Given: BAT1_R_INTERNAL -1 (estimate), same 15.2 V / 10 A sample
// When : one connected sample
// Then : the compensation uses the non-negative resistance estimate instead of the parameter, so the result lies
//        strictly above the uncompensated 0.5 and the estimate itself is reported as >= 0
TEST_F(SqeBatteryTest, BAT08_EstimatedInternalResistance_UsedWhenParameterNegative)
{
	setFloat("BAT1_R_INTERNAL", -1.f);
	TestableBattery b(1);
	stepConnected(b, 15.2f, 10.f, 1);

	const battery_status_s s = b.getBatteryStatus();
	EXPECT_GT(s.remaining, 0.5f);
	EXPECT_LT(s.remaining, 1.f);
	EXPECT_GE(s.internal_resistance_estimate, 0.f);
}

// SQE-BAT-09 | BAT-D15 F (no current, no compensation)
// Given: 15.2 V and 0 A
// When : one connected sample
// Then : no load compensation: (3.8-3.5)/0.6 = 0.5
TEST_F(SqeBatteryTest, BAT09_ZeroCurrent_NoLoadCompensation)
{
	TestableBattery b(1);
	stepConnected(b, 15.2f, 0.f, 1);

	EXPECT_NEAR(b.getBatteryStatus().remaining, 0.5f, 1e-5f);
}

// SQE-BAT-10 | BAT-D14 T · BAT-D07 3rd operand F (estimator never initialised with 0 cells) · BAT-D10 F ·
//              BAT-D23 1st operand F · BAT-D24 F
// Given: BAT1_N_CELLS 0 (cell count unknown)
// When : one connected sample at 15.2 V
// Then : voltage-based SoC is undefined (-1), no over-voltage fault can be computed, and the voltage scale falls back
//        to 1 because V_CHARGED/0 is not finite
TEST_F(SqeBatteryTest, BAT10_NoCellCount_SocUnknownScaleFallback)
{
	setInt("BAT1_N_CELLS", 0);
	TestableBattery b(1);
	stepConnected(b, 15.2f, 0.f, 1);

	const battery_status_s s = b.getBatteryStatus();
	EXPECT_FLOAT_EQ(s.remaining, -1.f);
	EXPECT_FLOAT_EQ(s.volt_based_soc_estimate, -1.f);
	EXPECT_EQ(s.faults, 0);
	EXPECT_FLOAT_EQ(s.scale, 1.f);
}

// SQE-BAT-11 | BAT-D24 T (finite scale, inside and clipped by the [1, 1.3] constraint)
// Given: 14.8 V (3.7 V/cell) and, separately, 16.8 V (4.2 V/cell), both at 0 A
// When : one connected sample each
// Then : scale = V_CHARGED / cell voltage: 4.1/3.7 = 1.1081 (inside the band); 4.1/4.2 < 1 is clipped to 1
TEST_F(SqeBatteryTest, BAT11_VoltageScale_ComputedAndConstrained)
{
	TestableBattery mid(1);
	stepConnected(mid, 14.8f, 0.f, 1);
	EXPECT_NEAR(mid.getBatteryStatus().scale, 4.1f / 3.7f, 1e-4f);

	TestableBattery full(1);
	stepConnected(full, 16.8f, 0.f, 1);
	EXPECT_FLOAT_EQ(full.getBatteryStatus().scale, 1.f);
}

// SQE-BAT-12 | BAT-D23 T∧T then T∧F (over-voltage "spike" fault)
// Given: 4 cells at V_CHARGED 4.1 V: the fault threshold is 4 * 4.1 * 1.05 = 17.22 V
// When : status at 17.3 V and at 17.1 V
// Then : FAULT_SPIKES bit set only above the threshold
TEST_F(SqeBatteryTest, BAT12_OverVoltage_ReportedAsSpikeFault)
{
	TestableBattery over(1);
	stepConnected(over, 17.3f, 0.f, 1);
	EXPECT_EQ(over.getBatteryStatus().faults, (1 << battery_status_s::FAULT_SPIKES));

	TestableBattery ok(1);
	stepConnected(ok, 17.1f, 0.f, 1);
	EXPECT_EQ(ok.getBatteryStatus().faults, 0);
}

// SQE-BAT-13 | BAT-D20 T/F · BAT-D21 T/F · BAT-D22 T/F — boundary value analysis on every threshold
// Given: BAT_EMERGEN_THR 0.05, BAT_CRIT_THR 0.07, BAT_LOW_THR 0.15 (all comparisons are strict "<")
// When : determineWarning() just below and exactly at each threshold, and at full charge
// Then : 0.049 EMERGENCY · 0.05 CRITICAL · 0.069 CRITICAL · 0.07 LOW · 0.149 LOW · 0.15 NONE · 1.0 NONE
TEST_F(SqeBatteryTest, BAT13_WarningThresholds_BoundaryValues)
{
	TestableBattery b(1);

	EXPECT_EQ(b.determineWarning(0.049f), battery_status_s::WARNING_EMERGENCY);
	EXPECT_EQ(b.determineWarning(0.05f), battery_status_s::WARNING_CRITICAL);
	EXPECT_EQ(b.determineWarning(0.069f), battery_status_s::WARNING_CRITICAL);
	EXPECT_EQ(b.determineWarning(0.07f), battery_status_s::WARNING_LOW);
	EXPECT_EQ(b.determineWarning(0.149f), battery_status_s::WARNING_LOW);
	EXPECT_EQ(b.determineWarning(0.15f), battery_status_s::WARNING_NONE);
	EXPECT_EQ(b.determineWarning(1.f), battery_status_s::WARNING_NONE);
}

// SQE-BAT-14 | BAT-D12 F then T · BAT-D13 F then T (dt guard and 2 s clamp)
// Given: a fresh battery (no previous timestamp)
// When : updateDt(T0); sumDischarged(3.6 A) — then updateDt(T0+1 s); sumDischarged(3.6) — then updateDt(T0+6 s);
//        sumDischarged(3.6)
// Then : first call has no dt yet, nothing integrated (0 mAh); 3.6 A for 1 s = 3600 mA * 1/3600 h = 1 mAh;
//        the 5 s gap is clamped to 2 s, adding 2 mAh -> 3 mAh total
TEST_F(SqeBatteryTest, BAT14_CoulombCounting_DtGuardAndClamp)
{
	TestableBattery b(1);

	b.updateDt(T0);
	EXPECT_FLOAT_EQ(b.sumDischarged(3.6f), 0.f);

	b.updateDt(T0 + 1_s);
	EXPECT_NEAR(b.sumDischarged(3.6f), 1.f, 1e-5f);

	b.updateDt(T0 + 6_s);
	EXPECT_NEAR(b.sumDischarged(3.6f), 3.f, 1e-5f);
}

// SQE-BAT-15 | BAT-D19 T∧F (capacity known, not yet initialised) then T∧T (fusion)
// Given: capacity 1000 mAh, 16.4 V at 36 A (voltage-based SoC saturates at 1.0, so the voltage weight is 0), samples
//        1 s apart from T0
// When : 4 samples (T0..T0+3 s) then a 5th (T0+4 s)
// Then : each second integrates 36 A * 1 s = 10 mAh. Before initialisation the voltage SoC (1.0) is used. At T0+3 s
//        (first initialised sample) SoC = min(1 - 30/1000, 1.0 - 10/1000) = 0.97; at T0+4 s min(1-40/1000,
//        0.97-0.01) = 0.96 (hand-computed)
TEST_F(SqeBatteryTest, BAT15_CapacityKnown_CoulombCountingFusedWithVoltage)
{
	setFloat("BAT1_CAPACITY", 1000.f);
	TestableBattery b(1);

	stepConnected(b, 16.4f, 36.f, 3);
	EXPECT_NEAR(b.getBatteryStatus().remaining, 1.f, 1e-5f);

	b.updateBatteryStatus(T0 + 3_s);
	EXPECT_NEAR(b.getBatteryStatus().remaining, 0.97f, 1e-4f);
	EXPECT_NEAR(b.getBatteryStatus().discharged_mah, 30.f, 1e-3f);

	b.updateBatteryStatus(T0 + 4_s);
	EXPECT_NEAR(b.getBatteryStatus().remaining, 0.96f, 1e-4f);
}

// SQE-BAT-16 | BAT-D08 F (externally supplied state of charge)
// Given: setStateOfCharge(0.4), then setStateOfCharge(1.5)
// When : connected samples at a voltage that would give a different voltage-based SoC (15.2 V -> 0.5)
// Then : the injected value is reported unchanged (0.4); an out-of-range injection is clamped to 1
TEST_F(SqeBatteryTest, BAT16_ExternalStateOfCharge_NotOverwritten)
{
	TestableBattery b(1);
	b.setStateOfCharge(0.4f);
	stepConnected(b, 15.2f, 0.f, 2);
	EXPECT_FLOAT_EQ(b.getBatteryStatus().remaining, 0.4f);

	b.setStateOfCharge(1.5f);
	b.updateBatteryStatus(T0 + 5_s);
	EXPECT_FLOAT_EQ(b.getBatteryStatus().remaining, 1.f);
}

namespace
{
// Subscriptions to every battery_status instance; consume() drops data published before the action under test.
struct BatteryStatusWatch {
	uORB::Subscription s0{ORB_ID(battery_status), 0};
	uORB::Subscription s1{ORB_ID(battery_status), 1};
	uORB::Subscription s2{ORB_ID(battery_status), 2};
	uORB::Subscription s3{ORB_ID(battery_status), 3};
	uORB::Subscription *all[4] {&s0, &s1, &s2, &s3};
	void consume()
	{
		battery_status_s s{};

		for (auto *x : all) { while (x->update(&s)) {} }
	}
	bool anyUpdatedWithVoltage(float v)
	{
		battery_status_s s{};
		bool found = false;

		for (auto *x : all) {
			while (x->update(&s)) {
				if (fabsf(s.voltage_v - v) < 1e-3f) { found = true; }
			}
		}

		return found;
	}
};
}

// SQE-BAT-17 | BAT-D11 T (driver source == BAT1_SOURCE) then F (source mismatch)
// Given: BAT1_SOURCE 0; one battery created with source 0, another with source 1
// When : updateAndPublishBatteryStatus() on each with distinct voltages (15.1 V / 15.3 V)
// Then : only the matching-source battery's sample appears on battery_status
TEST_F(SqeBatteryTest, BAT17_PublishOnlyWhenSourceMatchesParameter)
{
	BatteryStatusWatch watch;
	watch.consume();

	TestableBattery match(1, 0);
	match.setConnected(true);
	match.updateVoltage(15.1f);
	match.updateAndPublishBatteryStatus(T0);
	EXPECT_TRUE(watch.anyUpdatedWithVoltage(15.1f));

	TestableBattery other(1, 1);
	other.setConnected(true);
	other.updateVoltage(15.3f);
	other.updateAndPublishBatteryStatus(T0);
	EXPECT_FALSE(watch.anyUpdatedWithVoltage(15.3f));
}

// SQE-BAT-18 | BAT-D32 T · BAT-D29 F(disarmed) · BAT-D28 T(2nd operand: filter starts at 0) · BAT-D25 T · BAT-D26 T
// Given: capacity 5000 mAh, injected SoC 0.5, disarmed, BAT_AVRG_CURRENT 15 A
// When : computeRemainingTime(10 A)
// Then : the empty average filter is seeded with the 15 A parameter and not updated while disarmed:
//        0.5 * 5000 mAh / 15000 mA * 3600 s/h = 600 s
TEST_F(SqeBatteryTest, BAT18_RemainingTime_DisarmedUsesParameterAverage)
{
	TestableBattery b(1);
	b.setCapacityMah(5000.f);
	b.setStateOfCharge(0.5f);

	EXPECT_NEAR(b.computeRemainingTime(10.f), 600.f, 1e-2f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);
}

// SQE-BAT-19 | BAT-D32 F (capacity unknown)
// Given: capacity 0 (BAT1_CAPACITY -1 is clamped to 0)
// When : computeRemainingTime(10 A)
// Then : remaining time is undefined (NaN)
TEST_F(SqeBatteryTest, BAT19_RemainingTime_UnknownCapacityIsNaN)
{
	TestableBattery b(1);
	EXPECT_TRUE(std::isnan(b.computeRemainingTime(10.f)));
}

// SQE-BAT-20 | BAT-D29 T∧T · BAT-D30 T(1st operand: not FW) · BAT-D31 T · BAT-D27 F(rotary) · BAT-D25 F(no new status)
// Given: armed multicopter; dt = 1 s (two updateDt calls); BAT_AVRG_CURRENT 15 A; time constant 50 s
// When : computeRemainingTime(5 A), then again without a new vehicle_status
// Then : alpha = dt/(tau+dt) = 1/51; average = 15 + (5-15)/51 = 14.80392 A after the first call and
//        14.80392 + (5-14.80392)/51 = 14.61168 A after the second (hand-computed AlphaFilter steps)
TEST_F(SqeBatteryTest, BAT20_ArmedMulticopter_AverageCurrentFilteredWithDt)
{
	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	TestableBattery b(1);
	b.updateDt(T0);
	b.updateDt(T0 + 1_s);

	b.computeRemainingTime(5.f);
	const float a1 = 15.f + (5.f - 15.f) / 51.f;
	EXPECT_NEAR(b.getCurrentAverage(), a1, 1e-4f);

	b.computeRemainingTime(5.f);
	EXPECT_NEAR(b.getCurrentAverage(), a1 + (5.f - a1) / 51.f, 1e-4f);
}

// SQE-BAT-21 | BAT-D31 F (dt not yet known: fixed alpha from the constructor's sample interval)
// Given: armed multicopter, no updateDt call (dt = 0), sample interval 100 ms -> alpha = 0.1/(50+0.1)
// When : computeRemainingTime(5 A)
// Then : average = 15 + (5-15) * 0.1/50.1 = 14.98004 A; a negative current sample is clamped to 0 A before filtering
TEST_F(SqeBatteryTest, BAT21_ArmedWithoutDt_UsesConstructorAlphaAndClampsNegative)
{
	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	TestableBattery b(1);
	const float alpha = 0.1f / 50.1f;

	b.computeRemainingTime(5.f);
	const float a1 = 15.f + (5.f - 15.f) * alpha;
	EXPECT_NEAR(b.getCurrentAverage(), a1, 1e-4f);

	b.computeRemainingTime(-3.f);
	EXPECT_NEAR(b.getCurrentAverage(), a1 + (0.f - a1) * alpha, 1e-4f);
}

// SQE-BAT-22 | BAT-D29 T∧F (armed but the current sample is not finite)
// Given: armed multicopter, dt = 1 s
// When : computeRemainingTime(NaN)
// Then : the average is not touched (stays at the 15 A seed)
TEST_F(SqeBatteryTest, BAT22_ArmedNonFiniteCurrent_AverageNotUpdated)
{
	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	TestableBattery b(1);
	b.updateDt(T0);
	b.updateDt(T0 + 1_s);

	b.computeRemainingTime(std::numeric_limits<float>::quiet_NaN());
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);
}

// SQE-BAT-23 | BAT-D27 T (transition to FW) then F (already FW) · BAT-D28 T(3rd operand) · BAT-D30 F∧F(phase unknown)
// Given: armed multicopter whose average has been pulled below 15 A (two 5 A updates, dt = 1 s)
// When : vehicle_status switches to fixed-wing (VTOL transition), then a second fixed-wing status arrives
// Then : the transition resets the average to the 15 A parameter; the FW sample is not used because the flight phase
//        is not "level"; the second FW status does not reset again (average stays exactly 15 A)
TEST_F(SqeBatteryTest, BAT23_TransitionToFixedWing_ResetsAverageOnce)
{
	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	TestableBattery b(1);
	b.updateDt(T0);
	b.updateDt(T0 + 1_s);
	b.computeRemainingTime(5.f);
	b.computeRemainingTime(5.f);
	ASSERT_LT(b.getCurrentAverage(), 14.7f);

	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_FIXED_WING);
	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);

	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_FIXED_WING);
	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);
}

// SQE-BAT-24 | BAT-D30 F∨(T∧T) (FW, recent level flight) · F∨(F∧·) (FW, phase estimate older than 2 s) ·
//              F∨(T∧F) (FW, recent but climbing)
// Given: armed fixed-wing, dt = 1 s, average seeded at 15 A
// When : computeRemainingTime(5 A) with (a) a fresh LEVEL phase, (b) a LEVEL phase stamped 3 s ago, (c) a fresh
//        CLIMB phase
// Then : only (a) filters the sample (15 -> 14.80392 A); (b) and (c) leave the average unchanged
TEST_F(SqeBatteryTest, BAT24_FixedWing_OnlyRecentLevelFlightUpdatesAverage)
{
	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_FIXED_WING);
	TestableBattery b(1);
	b.updateDt(T0);
	b.updateDt(T0 + 1_s);

	publishPhase(flight_phase_estimation_s::FLIGHT_PHASE_LEVEL, hrt_absolute_time() - 3_s);
	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);

	publishPhase(flight_phase_estimation_s::FLIGHT_PHASE_CLIMB, hrt_absolute_time());
	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);

	publishPhase(flight_phase_estimation_s::FLIGHT_PHASE_LEVEL, hrt_absolute_time());
	b.computeRemainingTime(5.f);
	EXPECT_NEAR(b.getCurrentAverage(), 15.f + (5.f - 15.f) / 51.f, 1e-4f);
}

// SQE-BAT-25 | BAT-D28 T(1st operand: non-finite average) · BAT-D33 F · BAT-D34 F
// Given: BAT_AVRG_CURRENT set to NaN (a corrupt parameter) before construction
// When : computeRemainingTime() seeds the filter with NaN; then the parameter is corrected to 12 A, updateParams()
//        is called through the seam and computeRemainingTime() runs again
// Then : getCurrentAverage() reports the "invalid" value -1 while the average is NaN; the next call detects the
//        non-finite average and re-seeds it from the corrected parameter (12 A)
TEST_F(SqeBatteryTest, BAT25_NonFiniteAverage_ReseededFromParameter)
{
	setFloat("BAT_AVRG_CURRENT", std::numeric_limits<float>::quiet_NaN());
	TestableBattery b(1);

	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), -1.f);

	setFloat("BAT_AVRG_CURRENT", 12.f);
	b.updateParams();
	b.computeRemainingTime(5.f);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 12.f);
}

// SQE-BAT-26 | BAT-D26 F (copy() fails right after updated()) — link-time fake of orb_data_copy
// Given: an ARMED vehicle_status is published; the fake is armed to fail the next copy of vehicle_status
// When : computeRemainingTime(5 A) with dt = 1 s
// Then : the fake was hit exactly once; the armed state was not taken over, so the average is NOT filtered (stays
//        15 A). Positive control: the same status published again without the fault arms the battery and the next
//        call filters the sample (14.80392 A)
TEST_F(SqeBatteryTest, BAT26_VehicleStatusCopyFails_ArmingNotApplied)
{
	TestableBattery b(1);
	b.updateDt(T0);
	b.updateDt(T0 + 1_s);
	b.computeRemainingTime(5.f); // consumes the neutral SetUp status
	ASSERT_FLOAT_EQ(b.getCurrentAverage(), 15.f);

	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	g_copy_fail_topic = ORB_ID(vehicle_status);
	b.computeRemainingTime(5.f);
	EXPECT_EQ(g_copy_fail_hits, 1);
	EXPECT_FLOAT_EQ(b.getCurrentAverage(), 15.f);

	publishStatus(true, vehicle_status_s::VEHICLE_TYPE_ROTARY_WING);
	b.computeRemainingTime(5.f);
	EXPECT_NEAR(b.getCurrentAverage(), 15.f + (5.f - 15.f) / 51.f, 1e-4f);
}

// SQE-BAT-27 | BAT-D33 T then F · BAT-D34 T/F · BAT-D35 T/F — parameter re-read through the seam
// Given: a battery constructed with 4 cells (estimator initialised: covariance norm = n*sqrt(1.5^2+0.1^2))
// When : (a) BAT1_N_CELLS 6 + updateParams(); (b) updateParams() again unchanged; (c) BAT1_N_CELLS 0 + updateParams()
// Then : the norm is re-initialised for the new cell count only in (a): 4*1.50333 -> 6*1.50333; (b) leaves it as is;
//        (c) marks the estimator uninitialised but cannot re-initialise it with 0 cells (norm unchanged), cell_count 0
TEST_F(SqeBatteryTest, BAT27_CellCountChange_ReinitialisesResistanceEstimator)
{
	TestableBattery b(1);
	const float per_cell = sqrtf(1.5f * 1.5f + 0.1f * 0.1f);
	EXPECT_FALSE(b.firstParameterUpdate());
	EXPECT_NEAR(b.getBatteryStatus().estimation_covariance_norm, 4.f * per_cell, 1e-4f);

	setInt("BAT1_N_CELLS", 6);
	b.updateParams();
	EXPECT_EQ(b.cell_count(), 6);
	EXPECT_NEAR(b.getBatteryStatus().estimation_covariance_norm, 6.f * per_cell, 1e-4f);

	b.updateParams();
	EXPECT_NEAR(b.getBatteryStatus().estimation_covariance_norm, 6.f * per_cell, 1e-4f);

	setInt("BAT1_N_CELLS", 0);
	b.updateParams();
	EXPECT_EQ(b.cell_count(), 0);
	EXPECT_NEAR(b.getBatteryStatus().estimation_covariance_norm, 6.f * per_cell, 1e-4f);
}

// SQE-BAT-28 | BAT-D07 3rd operand F (estimator not initialised) · BAT-D14 T
// Given: battery constructed with 0 cells, so the internal-resistance estimator is never initialised
// When : BAT1_N_CELLS is raised to 4 *after* construction (no updateParams()), then a connected sample is taken
// Then : the settling-phase reset is skipped (estimator not initialised) and, since the cached cell count is still 0,
//        the state of charge stays undefined (-1)
TEST_F(SqeBatteryTest, BAT28_EstimatorNotInitialised_SettlingResetSkipped)
{
	setInt("BAT1_N_CELLS", 0);
	TestableBattery b(1);
	setInt("BAT1_N_CELLS", 4);

	stepConnected(b, 15.2f, 5.f, 1);
	EXPECT_FLOAT_EQ(b.getBatteryStatus().remaining, -1.f);
	EXPECT_FLOAT_EQ(b.getBatteryStatus().estimation_covariance_norm, 0.f);
}

// SQE-BAT-29 | BAT-D17 T and F (RLS update accepted / rejected) — behavioural oracle on the estimator
// Given: BAT1_R_INTERNAL -1, 4 cells; a synthetic pack obeying V = 16.4 V - 0.08 Ohm * I (0.02 Ohm/cell); current
//        excitation I_k = 10 + 8 sin(0.7 k) A, sampled every 100 ms for 60 s
// When : updateBatteryStatus() on each sample
// Then : after settling, the estimator has converged towards the true pack: per-cell resistance within 0.02 +/- 0.002
//        Ohm and open-circuit estimate within 16.4 +/- 0.05 V (the oracle is the synthetic model, not the code)
TEST_F(SqeBatteryTest, BAT29_ResistanceEstimator_ConvergesToSyntheticPack)
{
	setFloat("BAT1_R_INTERNAL", -1.f);
	TestableBattery b(1);
	b.setConnected(true);

	for (int k = 0; k < 600; k++) {
		const float i = 10.f + 8.f * sinf(0.7f * static_cast<float>(k));
		b.updateCurrent(i);
		b.updateVoltage(16.4f - 0.08f * i);
		b.updateBatteryStatus(T0 + static_cast<hrt_abstime>(k) * 100_ms);
	}

	const battery_status_s s = b.getBatteryStatus();
	RecordProperty("r_est", std::to_string(s.internal_resistance_estimate));
	RecordProperty("ocv_est", std::to_string(s.ocv_estimate));
	EXPECT_NEAR(s.internal_resistance_estimate, 0.02f, 0.002f);
	EXPECT_NEAR(s.ocv_estimate, 16.4f, 0.05f);
}

// SQE-BAT-30 | status field mapping (getBatteryStatus) — temperature, priority, capacity, cell count
// Given: temperature 25.5 C, priority 3, capacity 5000 mAh
// When : getBatteryStatus()
// Then : the values are reported unchanged in the published structure
TEST_F(SqeBatteryTest, BAT30_StatusFields_MappedFromInputs)
{
	TestableBattery b(1);
	b.updateTemperature(25.5f);
	b.setPriority(3);
	b.setCapacityMah(5000.f);

	const battery_status_s s = b.getBatteryStatus();
	EXPECT_FLOAT_EQ(s.temperature, 25.5f);
	EXPECT_EQ(s.priority, 3);
	EXPECT_EQ(s.capacity, 5000);
	EXPECT_EQ(s.cell_count, 4);
}

// SQE-BAT-31 | BAT-D16 T and BAT-D18 T at the boundary (BAT1_R_INTERNAL = 0 is a valid user value, ">= 0")
// Given: BAT1_R_INTERNAL exactly 0 Ohm (user states the pack has negligible resistance), 15.2 V at 10 A
// When : one connected sample
// Then : the user value 0 is used, i.e. no load compensation at all: (3.8-3.5)/0.6 = 0.5 exactly as with 0 A — the
//        online estimate (which is > 0 and would push the value above 0.5, see BAT08) must not be used
TEST_F(SqeBatteryTest, BAT31_ZeroUserInternalResistance_NoCompensation)
{
	setFloat("BAT1_R_INTERNAL", 0.f);
	TestableBattery b(1);
	stepConnected(b, 15.2f, 10.f, 1);

	EXPECT_NEAR(b.getBatteryStatus().remaining, 0.5f, 1e-5f);
}
