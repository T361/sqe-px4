/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : DataValidator.cpp / DataValidator.hpp (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest unit — justification: DataValidator has no params/uORB, pure explicit-argument API
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (DV-D01..DV-D15, REF_03)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "DataValidator.hpp"

namespace
{
constexpr uint64_t T0 = 1'000'000;
}

class SqeDataValidatorTest : public ::testing::Test
{
protected:
	DataValidator _dv;
};

// SQE-DV-01 | DV-D01F DV-D02F DV-D03 DV-D04T DV-D05T DV-D07F-D11F DV-D12T DV-D13T
// Given: a fresh DataValidator
// When : put(T0, {1,2,3}, error_count=0, priority=7) then confidence(T0)
// Then : used() true; value() == {1,2,3}; priority() == 7; error_count() == 0; confidence(T0) == 1; state() == 0;
//        rms() == {0,0,0} (first sample only initialises Welford state, never computes rms)
TEST_F(SqeDataValidatorTest, DV01_FirstSample_InitialisesAndReportsFullConfidence)
{
	const float val[3] = {1.f, 2.f, 3.f};

	_dv.put(T0, val, 0, 7);

	EXPECT_TRUE(_dv.used());
	EXPECT_FLOAT_EQ(_dv.value()[0], 1.f);
	EXPECT_FLOAT_EQ(_dv.value()[1], 2.f);
	EXPECT_FLOAT_EQ(_dv.value()[2], 3.f);
	EXPECT_EQ(_dv.priority(), 7);
	EXPECT_EQ(_dv.error_count(), 0u);

	EXPECT_FLOAT_EQ(_dv.confidence(T0), 1.f);
	EXPECT_EQ(_dv.state(), 0u);
	EXPECT_FLOAT_EQ(_dv.rms()[0], 0.f);
	EXPECT_FLOAT_EQ(_dv.rms()[1], 0.f);
	EXPECT_FLOAT_EQ(_dv.rms()[2], 0.f);
}

// SQE-DV-02 | DV-D05F DV-D06T/F
// Given: a fresh DataValidator
// When : put(T0,{1,0,0}) then put(T0+1000,{3,0,0})
// Then : rms()[0] == sqrt(2) (Welford sample variance of the low-pass deviations {0,2}, derived independently of the
//        production expression: lp0 after sample1 = 1.0, lp_val = 3-1.0 = 2.0, mean=1.0, M2=2.0, rms=sqrt(2.0/1));
//        rms()[1] == rms()[2] == 0 (constant axis)
TEST_F(SqeDataValidatorTest, DV02_SecondSample_ComputesWelfordRms)
{
	const float val1[3] = {1.f, 0.f, 0.f};
	const float val2[3] = {3.f, 0.f, 0.f};

	_dv.put(T0, val1, 0, 0);
	_dv.put(T0 + 1000, val2, 0, 0);

	EXPECT_NEAR(_dv.rms()[0], 1.41421356f, 1e-5f);
	EXPECT_FLOAT_EQ(_dv.rms()[1], 0.f);
	EXPECT_FLOAT_EQ(_dv.rms()[2], 0.f);
}

// SQE-DV-03 | DV-D09 boundary (F/T)
// Given: equal-value threshold set to 6; constant value {5,5,5}
// When : put() three times (shared equal-count reaches exactly 6) then confidence(); then a fourth put() (count 9)
// Then : after 3 puts confidence == 1 and state == 0 (6 > 6 is False, boundary not yet stale);
//        after the 4th put confidence == 0 and state == STALE (9 > 6 is True)
TEST_F(SqeDataValidatorTest, DV03_EqualCountThreshold_BoundaryNotStaleThenStale)
{
	_dv.set_equal_value_threshold(6);
	const float val[3] = {5.f, 5.f, 5.f};

	_dv.put(T0, val, 0, 0);
	_dv.put(T0 + 1000, val, 0, 0);
	_dv.put(T0 + 2000, val, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 2000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);

	_dv.put(T0 + 3000, val, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 3000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_STALE_DATA);
}

// SQE-DV-04 | DV-D06F DV-D09T/F DV-D13T
// Given: equal-value threshold set to 2; constant value {5,5,5} twice (goes stale), then a put that differs on axis 2
// When : confidence() after the 2 stale puts; then put({5,5,6}) and confidence() again
// Then : first confidence == 0 with STALE; after the differing put the shared equal-count resets to 0 at axis 2
//        (axes 0,1 still match and increment to 4,5 but axis 2 breaks the run back to 0), so confidence == 1 and
//        state() == 0 (error mask cleared because no critical error remains)
TEST_F(SqeDataValidatorTest, DV04_DifferingAxisResetsEqualCountAndClearsStale)
{
	_dv.set_equal_value_threshold(2);
	const float val_same[3] = {5.f, 5.f, 5.f};
	const float val_diff[3] = {5.f, 5.f, 6.f};

	_dv.put(T0, val_same, 0, 0);
	_dv.put(T0 + 1000, val_same, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_STALE_DATA);

	_dv.put(T0 + 2000, val_diff, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 2000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);
}

// SQE-DV-05 | DV-D06T DV-D09T/F
// Given: default equal-value threshold (100); scalar put(t, 7.f, 0, 1) — pads axes 1,2 with 0 every call
// When : 34 puts (shared equal-count reaches 3*33=99) then confidence(); then a 35th put (count 3*34=102)
// Then : value()[1]==value()[2]==0 (scalar put padding, DataValidator.cpp L49); after 34 puts confidence==1,
//        state==0 (99 > 100 False); after 35 puts confidence==0 and state==STALE (102 > 100 True).
//        The threshold default comment states the count is "accumulated also between axes" (VALUE_EQUAL_COUNT_DEFAULT,
//        DataValidator.hpp) — this test demonstrates exactly that shared-counter behaviour.
TEST_F(SqeDataValidatorTest, DV05_ScalarPut_PadsAxesAndSharesEqualCountAcrossAxes)
{
	for (int i = 0; i < 34; i++) {
		_dv.put(T0 + static_cast<uint64_t>(i) * 1000, 7.f, 0, 1);
	}

	EXPECT_FLOAT_EQ(_dv.value()[1], 0.f);
	EXPECT_FLOAT_EQ(_dv.value()[2], 0.f);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 34000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);

	_dv.put(T0 + 34000, 7.f, 0, 1);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 34000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_STALE_DATA);
}

// SQE-DV-06 | DV-D04F
// Given: a fresh DataValidator; put(T0,{1,2,3})
// When : put(T0+1000, {NaN, 5, +Inf})
// Then : value() == {1,5,3} — non-finite axes (0 and 2) are left untouched by DV-D04's PX4_ISFINITE guard; only
//        the finite axis (1) is updated
TEST_F(SqeDataValidatorTest, DV06_NonFiniteAxesAreIgnoredOnUpdate)
{
	const float val1[3] = {1.f, 2.f, 3.f};
	_dv.put(T0, val1, 0, 0);

	const float val2[3] = {std::numeric_limits<float>::quiet_NaN(), 5.f, std::numeric_limits<float>::infinity()};
	_dv.put(T0 + 1000, val2, 0, 0);

	EXPECT_FLOAT_EQ(_dv.value()[0], 1.f);
	EXPECT_FLOAT_EQ(_dv.value()[1], 5.f);
	EXPECT_FLOAT_EQ(_dv.value()[2], 3.f);
}

// SQE-DV-07 | DV-D04F; DV-D05 not reached
// Given: a fresh DataValidator
// When : put(t, {NaN,NaN,NaN}, 0, 1) ten times
// Then : characterization (F-09) — all three axes stay non-finite on every call, so DV-D04 is False every time and
//        the per-axis body (including DV-D05's init) never runs; value() stays {0,0,0} (default-initialised, never
//        written); confidence() == 1 and state() == 0 because _time_last is still updated unconditionally at the
//        end of put(), so used()/confidence() behave as if the stream were healthy despite carrying no real data.
TEST_F(SqeDataValidatorTest, DV07_AllNonFiniteStream_LeavesValueAtZeroButReportsFullConfidence)
{
	const float val[3] = {
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::quiet_NaN()
	};

	for (int i = 0; i < 10; i++) {
		_dv.put(T0 + static_cast<uint64_t>(i) * 1000, val, 0, 1);
	}

	EXPECT_TRUE(_dv.used());
	EXPECT_FLOAT_EQ(_dv.value()[0], 0.f);
	EXPECT_FLOAT_EQ(_dv.value()[1], 0.f);
	EXPECT_FLOAT_EQ(_dv.value()[2], 0.f);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 9000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);
}

// SQE-DV-08 | DV-D01T/F DV-D02T
// Given: a fresh DataValidator
// When : put(T0,v,error_count=10) then confidence(T0); put(T0+1000,v',error_count=10) then confidence(T0+1000)
// Then : first confidence == 0.90 (density jumps from 0 to 10 on the increase, DV-D01 True); second confidence ==
//        0.91 (error_count_in(10) > _error_count(10) is False, so DV-D02's "density>0" branch decrements to 9)
TEST_F(SqeDataValidatorTest, DV08_RepeatedErrorCount_DensityDecaysByOne)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 10, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.90f);

	_dv.put(T0 + 1000, val, 10, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 0.91f);
}

// SQE-DV-09 | DV-D02F
// Given: a fresh DataValidator
// When : put(T0,v,error_count=0) then confidence(T0); put(T0+1000,v',error_count=0) then confidence(T0+1000)
// Then : confidence == 1 both times — error density is floored at 0 by DV-D02's else-if guard (never decremented
//        below 0 since error_count_in stays at 0, so DV-D01 is False and DV-D02's "density>0" is also False)
TEST_F(SqeDataValidatorTest, DV09_ZeroErrorCount_DensityStaysFlooredAtZero)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 0, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 1.f);

	_dv.put(T0 + 1000, val, 0, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 1.f);
}

// SQE-DV-10 | DV-D01F DV-D02T
// Given: a fresh DataValidator
// When : put(T0,v,error_count=10); put(T0+1000,v',error_count=3)
// Then : error_count() == 3 (unconditionally overwritten every put); confidence(T0+1000) == 0.91 — a decreasing
//        error count is treated as "no new errors" (DV-D01 False because 3 > 10 is False) so density decays by one
//        via DV-D02 (10 -> 9) rather than jumping to 3
TEST_F(SqeDataValidatorTest, DV10_DecreasingErrorCount_StillDecaysDensityByOne)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 10, 0);
	_dv.put(T0 + 1000, val, 3, 0);

	EXPECT_EQ(_dv.error_count(), 3u);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 0.91f);
}

// SQE-DV-11 | DV-D07T DV-D12F
// Given: a fresh DataValidator (no put() ever called)
// When : confidence(T0)
// Then : confidence == 0 and state() == NO_DATA (DV-D07 True short-circuits the whole chain; DV-D12's "ret>0" is
//        False so the error mask is never cleared, leaving NO_DATA set)
TEST_F(SqeDataValidatorTest, DV11_NeverPut_ReportsNoData)
{
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_NO_DATA);
}

// SQE-DV-12 | DV-D08F/T DV-D13T
// Given: a fresh DataValidator; put(T0,v); default timeout (40000 us)
// When : confidence(T0+40000) and confidence(T0+40001); then set_timeout(1000); confidence(T0+1000) and
//        confidence(T0+1001)
// Then : at exactly the timeout boundary (40000) confidence == 1 and state == 0 (40000 > 40000 is False);
//        one microsecond past it confidence == 0 and state == TIMEOUT; get_timeout() == 1000 after set_timeout();
//        at the new boundary confidence == 1 again (TIMEOUT cleared because no new put was needed — the old
//        _time_last is still within the new, shorter window) and one microsecond past that confidence == 0 + TIMEOUT
TEST_F(SqeDataValidatorTest, DV12_TimeoutBoundary_ExactlyAtIntervalIsNotTimedOut)
{
	const float val[3] = {1.f, 1.f, 1.f};
	_dv.put(T0, val, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 40000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 40001), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_TIMEOUT);

	_dv.set_timeout(1000);
	EXPECT_EQ(_dv.get_timeout(), 1000u);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1001), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_TIMEOUT);
}

// SQE-DV-13 | DV-D10F/T DV-D11T DV-D13F DV-D12F
// Given: a fresh DataValidator
// When : put(T0,v,error_count=10000) then confidence(T0); put(T0+1000,v',error_count=10001) then confidence(T0+1000)
// Then : first confidence == 0 with state == HIGH_ERRDENSITY (density jumps to 10000, capped to 100 by DV-D11,
//        then 1-100/100==0 so DV-D13's "ret>0" is False and the HIGH_ERRDENSITY flag survives); second confidence
//        == 0 with state == HIGH_ERRDENSITY|HIGH_ERRCOUNT (error_count 10001 > 10000 makes DV-D10 True, which sets
//        ret=0 directly and the mask — never cleared since ret never exceeded 0 — keeps accumulating)
TEST_F(SqeDataValidatorTest, DV13_ErrorCountAboveLimit_AccumulatesDensityAndCountFlags)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 10000, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_HIGH_ERRDENSITY);

	_dv.put(T0 + 1000, val, 10001, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_HIGH_ERRDENSITY | DataValidator::ERROR_FLAG_HIGH_ERRCOUNT);
}

// SQE-DV-14 | DV-D11T/F DV-D13T
// Given: a fresh DataValidator
// When : put(T0,v,error_count=101) then confidence(T0); put(T0+1000,v',error_count=101) then confidence(T0+1000)
// Then : first confidence == 0 with HIGH_ERRDENSITY (density 101 > 100 caps to 100, 1-100/100==0); second
//        confidence ~= 0.01 with state == 0 — proves the cap exists and decays: density(100) decrements to 99
//        (101 > 101 is False, DV-D01 False, DV-D02 "density>0" True), 99 is not > 100 so DV-D11 is False this time
//        and the mask is cleared (1-99/100 == 0.01 > 0)
TEST_F(SqeDataValidatorTest, DV14_DensityCapAtWindow_DecaysOnNextPut)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 101, 0);
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_HIGH_ERRDENSITY);

	_dv.put(T0 + 1000, val, 101, 0);
	EXPECT_NEAR(_dv.confidence(T0 + 1000), 0.01f, 1e-6f);
	EXPECT_EQ(_dv.state(), 0u);
}

// SQE-DV-15 | DV-D11F DV-D12T DV-D13F
// Given: a fresh DataValidator
// When : put(T0,v,error_count=100) then confidence(T0)
// Then : characterization (F-10) — confidence == 0 (1 - 100/100 == 0) but state() == 0: density lands exactly on
//        the window boundary (100 > 100 is False) so DV-D11's HIGH_ERRDENSITY flag is never raised, even though the
//        returned confidence is the minimum possible value. No flag is set to explain the zero confidence.
TEST_F(SqeDataValidatorTest, DV15_DensityExactlyAtWindow_ZeroConfidenceButNoFlag)
{
	const float val[3] = {1.f, 1.f, 1.f};

	_dv.put(T0, val, 100, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.f);
	EXPECT_EQ(_dv.state(), 0u);
}

// SQE-DV-16 | DV-D08T DV-D13T
// Given: a fresh DataValidator; put(T0,v)
// When : confidence(T0+50000) (times out); put(T0+60000,v'); confidence(T0+60000)
// Then : first confidence == 0 with state == TIMEOUT (T0+50000 > T0+40000); after a fresh put the stream is current
//        again, so the second confidence == 1 and state() == 0 (TIMEOUT cleared)
TEST_F(SqeDataValidatorTest, DV16_TimeoutThenFreshPut_RecoversFullConfidence)
{
	const float val[3] = {1.f, 1.f, 1.f};
	_dv.put(T0, val, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 50000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_TIMEOUT);

	const float val2[3] = {2.f, 2.f, 2.f};
	_dv.put(T0 + 60000, val2, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 60000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);
}

// SQE-DV-17 | DV-D07T DV-D10T
// Given: a fresh DataValidator
// When : confidence(T0) (never put); put(T0+1000,v,error_count=10001); confidence(T0+1000)
// Then : first state == NO_DATA; second state == NO_DATA|HIGH_ERRCOUNT — the error mask accumulates with |= across
//        confidence() calls while ret never exceeds 0, so the NO_DATA flag from the first call is never cleared
TEST_F(SqeDataValidatorTest, DV17_NoDataThenHighErrorCount_FlagsAccumulate)
{
	EXPECT_FLOAT_EQ(_dv.confidence(T0), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_NO_DATA);

	const float val[3] = {1.f, 1.f, 1.f};
	_dv.put(T0 + 1000, val, 10001, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_NO_DATA | DataValidator::ERROR_FLAG_HIGH_ERRCOUNT);
}

// SQE-DV-18 | DV-D14T
// Given: a fresh DataValidator (no put() ever called)
// When : print() with stdout captured
// Then : captured output contains "no data" (DV-D14's early-return branch; PX4_INFO_RAW resolves to printf on this
//        SITL test platform, confirmed via platforms/common/include/px4_platform_common/log.h)
TEST_F(SqeDataValidatorTest, DV18_PrintWithNoData_ReportsNoData)
{
	testing::internal::CaptureStdout();
	_dv.print();
	const std::string out = testing::internal::GetCapturedStdout();

	EXPECT_NE(out.find("no data"), std::string::npos) << "captured output: " << out;
}

// SQE-DV-19 | DV-D14F DV-D15
// Given: a fresh DataValidator; put(1000,v) (a timestamp far behind real hrt_absolute_time())
// When : print() with stdout captured
// Then : characterization (F-05) — output contains 3 occurrences of "val:" (one per axis, DV-D15's loop); print()'s
//        per-axis confidence(hrt_absolute_time()) call is a side effect that times the stream out against the real
//        clock (hrt_absolute_time() at test run time is far larger than 1000+40000 us), so state() afterwards is
//        TIMEOUT
TEST_F(SqeDataValidatorTest, DV19_PrintWithData_PrintsThreeLinesAndTimesOutAsSideEffect)
{
	const float val[3] = {1.f, 1.f, 1.f};
	_dv.put(1000, val, 0, 0);

	testing::internal::CaptureStdout();
	_dv.print();
	const std::string out = testing::internal::GetCapturedStdout();

	size_t count = 0;
	size_t pos = 0;

	while ((pos = out.find("val:", pos)) != std::string::npos) {
		count++;
		pos += 4;
	}

	EXPECT_EQ(count, 3u) << "captured output: " << out;
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_TIMEOUT);
}

// SQE-DV-20 | DV-D05T DV-D07T
// Given: a fresh DataValidator
// When : put(0,v) (timestamp 0); confidence(0); put(1000,v')
// Then : characterization (F-12) — used() == false after put(0,v) because _time_last is set to the timestamp
//        argument (0) at the end of put(), so the "ever saw data" check (_time_last > 0) stays False even though
//        value() == v; confidence(0) == 0 with state == NO_DATA for the same reason; the next put() re-enters the
//        DV-D05 init branch (because _time_last is still 0), so rms() stays {0,0,0} instead of computing a delta
TEST_F(SqeDataValidatorTest, DV20_TimestampZero_NeverMarksUsedAndReInitsOnNextPut)
{
	const float val1[3] = {1.f, 2.f, 3.f};
	_dv.put(0, val1, 0, 0);

	EXPECT_FALSE(_dv.used());
	EXPECT_FLOAT_EQ(_dv.confidence(0), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_NO_DATA);
	EXPECT_FLOAT_EQ(_dv.value()[0], 1.f);
	EXPECT_FLOAT_EQ(_dv.value()[1], 2.f);
	EXPECT_FLOAT_EQ(_dv.value()[2], 3.f);

	const float val2[3] = {4.f, 5.f, 6.f};
	_dv.put(1000, val2, 0, 0);

	EXPECT_FLOAT_EQ(_dv.rms()[0], 0.f);
	EXPECT_FLOAT_EQ(_dv.rms()[1], 0.f);
	EXPECT_FLOAT_EQ(_dv.rms()[2], 0.f);
}

// SQE-DV-21 | DV-D06 boundary (F at exactly the 1e-6 threshold, then T)
// Given: equal-value threshold 2 (stale when the shared counter exceeds 2); first sample {0,0,0}
// When : second sample {1e-6f,1e-6f,1e-6f}: each axis differs by exactly 0.000001f (0.0f - 1e-6f is exact in
//        float32, so the comparison sits on the boundary); third sample repeats {1e-6f,...}
// Then : after the 2nd sample the "< 0.000001f" test is False on every axis, the counter is reset to 0 and the
//        sensor is not stale (confidence 1, state 0); after the 3rd sample all 3 axes compare equal, the counter
//        reaches 3 > 2 and the sensor reports STALE (positive control proving the counter path is reachable)
TEST_F(SqeDataValidatorTest, DV21_EqualValueThresholdExactBoundary_NotCountedAsEqual)
{
	_dv.set_equal_value_threshold(2);
	const float zero[3] = {0.f, 0.f, 0.f};
	const float eps[3] = {0.000001f, 0.000001f, 0.000001f};

	_dv.put(T0, zero, 0, 0);
	_dv.put(T0 + 1000, eps, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 1000), 1.f);
	EXPECT_EQ(_dv.state(), 0u);

	_dv.put(T0 + 2000, eps, 0, 0);

	EXPECT_FLOAT_EQ(_dv.confidence(T0 + 2000), 0.f);
	EXPECT_EQ(_dv.state(), DataValidator::ERROR_FLAG_STALE_DATA);
}
