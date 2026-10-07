/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : DataValidatorGroup.cpp / DataValidatorGroup.hpp (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest unit — justification: DataValidatorGroup has no params/uORB, pure explicit-argument API
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs (DVG-D01..D38, REF_04);
 *                      MC/DC rows: work/mcdc/mcdc_matrix.csv (SQE-DVG-MC-01..25, gaps at 15/16/17)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "DataValidatorGroup.hpp"

namespace
{
constexpr uint64_t T0 = 1'000'000;
} // namespace

class SqeDvgTest : public ::testing::Test
{
};

class SqeDvgMcdcTest : public ::testing::Test
{
};

class SqeDvgDeathTest : public ::testing::Test
{
};

// ===========================================================================
// Structural catalogue — SQE-DVG-nn
// ===========================================================================

// SQE-DVG-01 | DVG-D01 DVG-D02T DVG-D03T DVG-D35 DVG-D36T/F DVG-D37 DVG-D38T/F DVG-D31 DVG-D33
// Given: a fresh DataValidatorGroup(1), never fed
// When : get_sensor_priority(0/1), get_sensor_state(0/1), failover_index(), failover_state()
// Then : prio(0)=0, state(0)=0 (never-put DataValidator state defaults to NO_ERROR); state(1)=UINT32_MAX
//        (index not found); prio(1)=0 (index not found); failover_index()==-1; failover_state()==NO_ERROR
TEST_F(SqeDvgTest, DVG01_FreshSingleSensor_QueriesReturnDefaultsAndNotFoundSentinels)
{
	DataValidatorGroup g(1);

	EXPECT_EQ(g.get_sensor_priority(0), 0u);
	EXPECT_EQ(g.get_sensor_state(0), 0u);
	EXPECT_EQ(g.get_sensor_state(1), UINT32_MAX);
	EXPECT_EQ(g.get_sensor_priority(1), 0u);
	EXPECT_EQ(g.failover_count(), 0u);
	EXPECT_EQ(g.failover_index(), -1);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_NO_ERROR);
}

// SQE-DVG-02 | DVG-D02F DVG-D08 DVG-D09T/F (loop exit without break)
// Given: a fresh DataValidatorGroup(3)
// When : put(2, T0, v, 0, 42) then an out-of-range put(7, T0, v, 0, 99)
// Then : sensor 2's priority is 42 (found, index matches mid-loop); sensors 0/1 stay at priority 0 (never put);
//        the out-of-range put(7,...) is silently ignored (loop runs to the end without ever matching, no crash,
//        no state changed) — confirmed by priorities being unchanged after it
TEST_F(SqeDvgTest, DVG02_PutAtValidIndexSetsPriority_OutOfRangePutIgnored)
{
	DataValidatorGroup g(3);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(2, T0, val, 0, 42);

	EXPECT_EQ(g.get_sensor_priority(2), 42u);
	EXPECT_EQ(g.get_sensor_priority(0), 0u);
	EXPECT_EQ(g.get_sensor_priority(1), 0u);

	g.put(7, T0, val, 0, 99);

	EXPECT_EQ(g.get_sensor_priority(0), 0u);
	EXPECT_EQ(g.get_sensor_priority(1), 0u);
	EXPECT_EQ(g.get_sensor_priority(2), 42u);
}

// SQE-DVG-03 | DVG-D01 0-iteration DVG-D03F DVG-D06/D07/D10/D12 0-iteration
// Given: a DataValidatorGroup(0) (no siblings constructed, _first/_last stay nullptr)
// When : get_best(), failover_index()/failover_state(), get_sensor_state()/get_sensor_priority(), set_timeout(),
//        set_equal_value_threshold(), print()
// Then : get_best() returns nullptr and *index == -1 (both while loops in get_best() run zero iterations, best
//        stays nullptr, max_index stays -1); failover_index()==-1; failover_state()==NO_ERROR; get_sensor_state()
//        ==UINT32_MAX (not found); get_sensor_priority()==0 (not found); set_timeout()/set_equal_value_threshold()
//        do not crash on an empty group (0-iteration loops); print() emits exactly its header line for the empty
//        group — "validator: best: -1, prev best: -1, failsafe: NO (0 events)" (L250-251) — and no "sensor #" line,
//        because its per-sensor while loop runs zero iterations (L256)
TEST_F(SqeDvgTest, DVG03_EmptyGroup_AllQueriesReturnSentinelsNoCrash)
{
	DataValidatorGroup g(0);
	int idx = -99;

	float *best = g.get_best(T0, &idx);

	EXPECT_EQ(best, nullptr);
	EXPECT_EQ(idx, -1);
	EXPECT_EQ(g.failover_index(), -1);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_NO_ERROR);
	EXPECT_EQ(g.get_sensor_state(0), UINT32_MAX);
	EXPECT_EQ(g.get_sensor_priority(0), 0u);

	g.set_timeout(5000);
	g.set_equal_value_threshold(3);

	testing::internal::CaptureStdout();
	g.print();
	const std::string out = testing::internal::GetCapturedStdout();

	EXPECT_NE(out.find("validator: best: -1, prev best: -1, failsafe: NO (0 events)"), std::string::npos) << out;
	EXPECT_EQ(out.find("sensor #"), std::string::npos) << out;
}

// SQE-DVG-04 | DVG-D05F DVG-D06
// Given: a DataValidatorGroup(1) with default timeout; then set_timeout(250000)
// When : add_new_validator() is called after the timeout change
// Then : returned pointer is non-null; get_sensor_state(1) != UINT32_MAX (new sibling is found, i.e. it was
//        actually linked in via setSibling()); the new validator inherits the group's current _timeout_interval_us
//        (250000) — observed indirectly: it is NOT timed out at T0+240000 (within 250000) but times out at
//        T0+250001, proving set_timeout() propagated to it even though it didn't exist yet when set_timeout() ran
TEST_F(SqeDvgTest, DVG04_AddNewValidatorAfterSetTimeout_InheritsGroupTimeout)
{
	DataValidatorGroup g(1);

	g.set_timeout(250000);
	DataValidator *added = g.add_new_validator();

	ASSERT_NE(added, nullptr);
	EXPECT_NE(g.get_sensor_state(1), UINT32_MAX);

	const float val[3] = {1.f, 1.f, 1.f};
	g.put(1, T0, val, 0, 0);

	int idx = -99;
	g.get_best(T0 + 240000, &idx);
	EXPECT_NE(g.get_sensor_state(1), DataValidator::ERROR_FLAG_TIMEOUT);

	g.get_best(T0 + 250001, &idx);
	EXPECT_EQ(g.get_sensor_state(1), DataValidator::ERROR_FLAG_TIMEOUT);
}

// SQE-DVG-05 | DVG-D06
// Given: a DataValidatorGroup(2) with set_timeout(10000); both sensors fed once at T0
// When : get_best(T0 + 10001, &idx)
// Then : idx == -1 (both sensors time out simultaneously, confidence() returns 0 for both, D13's G=confidence>0.0f
//        is False for every sibling so max_index never advances past -1); both get_sensor_state() report TIMEOUT
TEST_F(SqeDvgTest, DVG05_GroupTimeoutShorterThanDefault_BothSensorsTimeOutTogether)
{
	DataValidatorGroup g(2);
	g.set_timeout(10000);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	g.put(1, T0, val, 0, 50);

	int idx = -99;
	float *best = g.get_best(T0 + 10001, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(best, nullptr);
	EXPECT_EQ(g.get_sensor_state(0), DataValidator::ERROR_FLAG_TIMEOUT);
	EXPECT_EQ(g.get_sensor_state(1), DataValidator::ERROR_FLAG_TIMEOUT);
}

// SQE-DVG-06 | DVG-D07
// Given: a DataValidatorGroup(2) with set_equal_value_threshold(2); both sensors fed the identical value 3x each
// When : get_best() after the staling puts
// Then : idx == -1 (both sensors go STALE, confidence 0 for both, same reasoning as DVG05); both get_sensor_state()
//        report STALE_DATA — proves set_equal_value_threshold() propagated to every sibling
TEST_F(SqeDvgTest, DVG06_GroupEqualValueThreshold_PropagatesToAllSiblingsAndStales)
{
	DataValidatorGroup g(2);
	g.set_equal_value_threshold(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	g.put(0, T0 + 1000, val, 0, 50);
	g.put(0, T0 + 2000, val, 0, 50);
	g.put(1, T0, val, 0, 50);
	g.put(1, T0 + 1000, val, 0, 50);
	g.put(1, T0 + 2000, val, 0, 50);

	int idx = -99;
	g.get_best(T0 + 2000, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(g.get_sensor_state(0), DataValidator::ERROR_FLAG_STALE_DATA);
	EXPECT_EQ(g.get_sensor_state(1), DataValidator::ERROR_FLAG_STALE_DATA);
}

// SQE-DVG-07 | DVG-D13=T twice (i=0 via A&&B, i=1 via E&&F)
// Given: a fresh DataValidatorGroup(2); sensor 0 (prio 10) and sensor 1 (prio 20) both put with d=0 (confidence 1.0)
//        at the same timestamp T0, as the very first get_best() call ever
// Then : idx == 1 — with equal confidence (1.0 for both, well above MIN_REGULAR_CONFIDENCE) sensor 1's higher
//        priority wins the tie (D13's E&&F path: |c-m|<0.01f true, priority>max_priority true)
TEST_F(SqeDvgTest, DVG07_EqualConfidenceDifferentPriority_HigherPriorityWins)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 10);
	g.put(1, T0, val, 0, 20);

	int idx = -99;
	g.get_best(T0, &idx);

	EXPECT_EQ(idx, 1);
}

// SQE-DVG-08 | DVG-D19F (second failover) DVG-D22T
// Given: continuing the MC14-style classic failover (sensor 0 silent, sensor 1 takes over, count becomes 1), then
//        sensor 1 also goes silent and sensor 0 comes back fresh, forcing a second failover
// Then : failover_count() == 2 after the second switch; print()'s captured stdout contains "failsafe: YES"
//        (DataValidatorGroup.cpp L250-251, `(_toggle_count > 0) ? "YES" : "NO"`)
TEST_F(SqeDvgTest, DVG08_SecondFailover_CountsTwiceAndPrintShowsFailsafeYes)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1, val, 0, 50);
	g.get_best(T0 + 1, &idx);
	ASSERT_EQ(idx, 0);

	// sensor 0 now silent since T0, sensor 1 fresh -> first failover to sensor 1
	g.put(1, T0 + 50000, val, 0, 50);
	g.get_best(T0 + 50000, &idx);
	ASSERT_EQ(idx, 1);
	ASSERT_EQ(g.failover_count(), 1u);

	// sensor 1 now goes silent since T0+50000, sensor 0 comes back fresh -> second failover to sensor 0
	g.put(0, T0 + 100000, val, 0, 50);
	g.get_best(T0 + 100000, &idx);
	ASSERT_EQ(idx, 0);

	EXPECT_EQ(g.failover_count(), 2u);

	testing::internal::CaptureStdout();
	g.print();
	const std::string out = testing::internal::GetCapturedStdout();

	EXPECT_NE(out.find("failsafe: YES"), std::string::npos) << "captured output: " << out;
}

// SQE-DVG-09 | DVG-D22F DVG-D23 DVG-D24T/F DVG-D25..D30 T/F
// Given: a DataValidatorGroup(5) crafted so that: #0 is healthy (OK), #1 goes STALE (identical value put repeatedly
//        past the equal-value threshold), #2 is put once with a huge error_count to set HIGH_ERRCOUNT, #3 is put
//        with a huge error_count to set HIGH_ERRDENSITY, #4 is left completely unused (never put, so used()==false
//        and print() skips it). STALE/ECNT/EDNST are deliberately on three *different* sensors: DataValidator::
//        confidence()'s checks are an if/else-if chain (DataValidator.cpp L106-129), so a single sensor can only
//        ever raise one of {NO_DATA, TIMEOUT, STALE_DATA, HIGH_ERRCOUNT, HIGH_ERRDENSITY} from any one confidence()
//        evaluation — stacking STALE and ECNT onto the same sensor in one call is infeasible by construction
//        (confirmed by re-running this test with that combination first: STALE always short-circuits ECNT because
//        the identical value needed to trigger STALE never lets _error_count's branch execute in the same call).
// When : print() with stdout captured
// Then : output shows "sensor #0" with " OK", does NOT show "sensor #4" (unused sensors are skipped by the
//        `next->used()` guard at L256), shows " STALE" for sensor #1, " ECNT" for sensor #2, " EDNST" for sensor
//        #3, and shows "failsafe: NO" (no failover has happened in this scenario, _toggle_count stays 0)
TEST_F(SqeDvgTest, DVG09_PrintSkipsUnusedSensorsAndShowsPerSensorFlags)
{
	DataValidatorGroup g(5);
	const float val[3] = {1.f, 1.f, 1.f};

	// #0: healthy
	g.put(0, T0, val, 0, 0);

	// #1: stale (identical value repeated past the equal-value threshold)
	g.set_equal_value_threshold(2);
	g.put(1, T0, val, 0, 1);
	g.put(1, T0 + 1000, val, 0, 1);
	g.put(1, T0 + 2000, val, 0, 1);

	// #2: high error count
	g.put(2, T0, val, 10001, 2);

	// #3: high error density
	g.put(3, T0, val, 10000, 3);

	int idx = -99;
	g.get_best(T0 + 2000, &idx);

	testing::internal::CaptureStdout();
	g.print();
	const std::string out = testing::internal::GetCapturedStdout();

	EXPECT_NE(out.find("sensor #0"), std::string::npos) << out;
	EXPECT_NE(out.find(" OK"), std::string::npos) << out;
	EXPECT_EQ(out.find("sensor #4"), std::string::npos) << out;
	EXPECT_NE(out.find(" STALE"), std::string::npos) << out;
	EXPECT_NE(out.find(" ECNT"), std::string::npos) << out;
	EXPECT_NE(out.find(" EDNST"), std::string::npos) << out;
	EXPECT_NE(out.find("failsafe: NO"), std::string::npos) << out;
}

// SQE-DVG-10 | DVG-D05 crash path — characterization F-03 (death test)
// Given: a DataValidatorGroup(0) (constructed with zero siblings, _last stays nullptr)
// When : add_new_validator() is called
// Then : the process crashes (SIGSEGV) because add_new_validator() unconditionally dereferences `_last`
//        (`_last->setSibling(validator)`, DataValidatorGroup.cpp L92) without checking it for null first — a
//        genuine, independently-confirmed characterization (REF_07 F-03), not a bug invented for this test
TEST_F(SqeDvgDeathTest, DVG10_AddNewValidatorOnEmptyGroup_CrashesOnNullLast)
{
	GTEST_FLAG_SET(death_test_style, "threadsafe");

	EXPECT_DEATH({
		DataValidatorGroup g(0);
		g.add_new_validator();
	}, "");
}

// SQE-DVG-11 | DVG-D14T (I&&J) DVG-D20T
// Given: a DataValidatorGroup(2); sensor 0 becomes best, then both sensors go silent past the timeout
// When : get_best() after both time out
// Then : idx == -1, failover_count() == 1 (the "only sensor went bad" branch of D14, I&&J true since max_confidence
//        < FLT_EPSILON and _curr_best >= 0); the returned pointer is non-null — characterization F-02: get_best()
//        returns the last-known values of the previously-best sensor even though *index is -1, which contradicts
//        the header's "pointer to the array of best values" phrasing when nothing is actually selected
TEST_F(SqeDvgTest, DVG11_BothSensorsTimeOutAfterOneWasBest_IndexNegativeButPointerNonNull)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	float *best = g.get_best(T0 + 50000, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(g.failover_count(), 1u);
	EXPECT_NE(best, nullptr);
}

// SQE-DVG-12 | DVG-D01 DVG-D02 DVG-D04
// Given/When: construct and destroy DataValidatorGroup instances of sizes 0..5, calling add_new_validator() once
//             extra on the size-3 instance before it goes out of scope
// Then : the constructor links exactly n siblings — get_sensor_state(i) is found (!= UINT32_MAX) for every i < n and
//        not found (== UINT32_MAX) at i == n; after add_new_validator() the size-3 group has a 4th reachable sibling
//        (index 3 found, index 4 not found). The destructor's while-loop (DVG-D04) then walks and deletes every linked
//        sibling, including the added one, for every size including 0 — exercised here for coverage; freeing itself is
//        not observable through the public API (a leak would need ASan/LeakSanitizer to detect), so the destructor part
//        of this test is a smoke check, not an oracle
TEST_F(SqeDvgTest, DVG12_ConstructDestroyVariousSizes_LinksExactlyNSiblings)
{
	for (unsigned n = 0; n <= 5; n++) {
		DataValidatorGroup g(n);

		for (unsigned i = 0; i < n; i++) {
			EXPECT_NE(g.get_sensor_state(i), UINT32_MAX) << "size " << n << ", index " << i;
		}

		EXPECT_EQ(g.get_sensor_state(n), UINT32_MAX) << "size " << n;

		if (n == 3) {
			ASSERT_NE(g.add_new_validator(), nullptr);
			EXPECT_NE(g.get_sensor_state(3), UINT32_MAX);
			EXPECT_EQ(g.get_sensor_state(4), UINT32_MAX);
		}
	}
}

// SQE-DVG-13 | DVG-D25 T (the " OFF" ternary, L261) — closes the former gap G-04
// Given: a DataValidatorGroup(1) whose sensor is queried by get_best() before it has ever been fed, so confidence()
//        sets ERROR_FLAG_NO_DATA in its error mask (DataValidator.cpp L106-108); the sensor is then fed once
// When : print() is called before any further confidence() evaluation (the mask is only rewritten by confidence())
// Then : the sensor is now used() (fed), so print() lists it, and its state still carries NO_DATA, so the line shows
//        " OFF" (L261's ternary True side) and not " OK"
TEST_F(SqeDvgTest, DVG13_PrintFedSensorWithStaleNoDataFlag_ShowsOff)
{
	DataValidatorGroup g(1);
	int idx = -99;

	g.get_best(T0, &idx); // never fed: confidence() sets NO_DATA
	ASSERT_EQ(idx, -1);
	ASSERT_EQ(g.get_sensor_state(0), DataValidator::ERROR_FLAG_NO_DATA);

	const float val[3] = {1.f, 1.f, 1.f};
	g.put(0, T0 + 1000, val, 0, 50); // now used(), mask untouched until the next confidence()

	testing::internal::CaptureStdout();
	g.print();
	const std::string out = testing::internal::GetCapturedStdout();

	EXPECT_NE(out.find("sensor #0, prio: 50, state: OFF"), std::string::npos) << out;
	EXPECT_EQ(out.find(" OK"), std::string::npos) << out;
}

// ===========================================================================
// MC/DC catalogue — SQE-DVG-MC-nn (fixture SqeDvgMcdcTest)
// Authoritative source: work/mcdc/mcdc_matrix.csv (22 distinct test IDs, 29 target-evaluation rows)
// ===========================================================================

// SQE-DVG-MC-01 | DVG-D13 (E,F true) · DVG-D15 (K,L true)
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50, seeded via call1); sensor 1 appears with d=5 (conf
//        .95, identical) and prio 75
// When : get_best() at T0+1000 (call2, target evaluation of D13 at i=1 and, in the same call, D15)
// Then : idx == 1 (|c-m|<0.01f true (E), p>mp true (F) -> switch); failover_count() stays 0 — the switch is a
//        same-confidence, higher-priority preference, not a failsafe (D15 true: K true, L true, M true -> true_failsafe=false)
TEST_F(SqeDvgMcdcTest, MC01_EqualConfidenceHigherPriority_SwitchNotCountedAsFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 75);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-02 | DVG-D13 (F false, pair of MC01 on F)
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50); sensor 1 appears with d=5 (conf .95), equal prio 50
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: F = p>mp = 50>50 = False, no switch); failover_count() stays 0
TEST_F(SqeDvgMcdcTest, MC02_EqualConfidenceEqualPriority_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 50);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-03 | DVG-D13 (E false, pair of MC01 on E)
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50); sensor 1 appears with d=7 (conf .93), prio 75
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: E = |.93-.95|=.02f >= 0.01f = False; C=.93>.95 False too, CD false; AB false since
//        neither confidence is below 0.9f)
TEST_F(SqeDvgMcdcTest, MC03_LowerConfidenceHigherPriority_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 7, 75);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-04 | DVG-D13 (C,D true) · DVG-D15 (L false) | F-11
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50); sensor 1 appears with d=3 (conf .97), equal prio 50
// When : get_best() at T0+1000
// Then : idx == 1 (D13 true via C&&D: confidence strictly higher, priority equal so >= holds); failover_count()
//        becomes 1 — D15 false (L: 50<50 is False) so true_failsafe stays True; characterization (F-11): a
//        confidence-only, same-priority improvement is counted as a failsafe by this code, which is a semantic
//        finding (see REF_07 F-11), not a test defect — the matrix records the real code behaviour per R4
TEST_F(SqeDvgMcdcTest, MC04_HigherConfidenceEqualPriority_SwitchCountedAsFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 3, 50);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_count(), 1u);
}

// SQE-DVG-MC-05 | DVG-D13 (D false, pair of MC04 on D)
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50); sensor 1 appears with d=3 (conf .97), lower prio 25
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: C true but D=25>=50 False -> CD false; E: |.97-.95|=.02f>=.01f also False)
TEST_F(SqeDvgMcdcTest, MC05_HigherConfidenceLowerPriority_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 3, 25);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-06 | DVG-D13 (C false, pair of MC04 on C)
// Given: sensor 0 is current best (d=5 -> conf .95, prio 50); sensor 1 appears with d=7 (conf .93), equal prio 50
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: C=.93>.95 False; E: |.93-.95|=.02f>=.01f False too)
TEST_F(SqeDvgMcdcTest, MC06_LowerConfidenceEqualPriority_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 5, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 7, 50);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-07 | DVG-D13 (A true, pair of MC08 on A)
// Given: sensor 0 is current best (d=11 -> conf .89999998f < 0.9f, prio 50); sensor 1 appears with d=5 (conf
//        .95 >= 0.9f), prio 25
// When : get_best() at T0+1000
// Then : idx == 1 (D13 true via A&&B: max_confidence below the regular-confidence threshold, candidate at/above it
//        -> switch regardless of priority); failover_count() becomes 1 (D15 unreachable observation not asserted
//        here; D13 is this row's target)
TEST_F(SqeDvgMcdcTest, MC07_BelowThresholdConfidenceReplacedByAboveThreshold_Switches)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 11, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 25);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_count(), 1u);
}

// SQE-DVG-MC-08 | DVG-D13 (A false, pair of MC07 on A)
// Given: sensor 0 is current best (d=9 -> conf .91000003f >= 0.9f, prio 50); sensor 1 appears with d=5 (conf .95),
//        prio 25
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: A=max_confidence<0.9f is False this time, so AB false; falls to C(.95>.91 true),
//        D(25>=50 false) -> CD false; E(|.95-.91|=.04f>=.01f false))
TEST_F(SqeDvgMcdcTest, MC08_AboveThresholdConfidenceLowerPriorityCandidate_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 9, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 25);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-09 | DVG-D13 (B true, pair of MC10 on B)
// Given: sensor 0 is current best (d=20 -> conf .80, prio 50); sensor 1 appears with d=5 (conf .95 >= 0.9f), prio 25
// When : get_best() at T0+1000
// Then : idx == 1 (D13 true via A&&B); failover_count() becomes 1
TEST_F(SqeDvgMcdcTest, MC09_FarBelowThresholdReplacedByAboveThreshold_Switches)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 20, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 25);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_count(), 1u);
}

// SQE-DVG-MC-10 | DVG-D13 (B false, pair of MC09 on B)
// Given: sensor 0 is current best (d=20 -> conf .80, prio 50); sensor 1 appears with d=15 (conf .85 < 0.9f), prio 25
// When : get_best() at T0+1000
// Then : idx == 0 (D13 false: B=confidence>=0.9f is False this time, AB false; C(.85>.80 true), D(25>=50 false)
//        -> CD false; E(|.85-.80|=.05f>=.01f false))
TEST_F(SqeDvgMcdcTest, MC10_BothBelowThresholdLowerPriorityCandidate_NoSwitch)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 20, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 15, 25);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-11 | DVG-D13 (G false, pair of MC12 on G)
// Given: a fresh DataValidatorGroup(2), sensor 0 never put()
// When : get_best(T0, &idx) as the very first call ever
// Then : idx == -1, returned pointer nullptr, failover_count() == 0 — D13's G = confidence>0.0f is False
//        (confidence()==0, ERROR_FLAG_NO_DATA) regardless of the inner OR expression, decision forced False
TEST_F(SqeDvgMcdcTest, MC11_FirstCallNeverPut_NoCandidateSelected)
{
	DataValidatorGroup g(2);

	int idx = -99;
	float *best = g.get_best(T0, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(best, nullptr);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-12 | DVG-D13 (G true, pair of MC11 on G)
// Given: a fresh DataValidatorGroup(2); sensor 0 put(d=50 -> conf .50, prio 50) just before the first-ever get_best()
// When : get_best(T0, &idx) as the very first call ever
// Then : idx == 0 (D13 true via C&&D: pre-loop max_confidence=-1.0f/max_priority=-1000 so C=.50>-1 true,
//        D=50>=-1000 true; G=confidence>0.0f true)
TEST_F(SqeDvgMcdcTest, MC12_FirstCallOneSensorPut_SelectsIt)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 50, 50);

	int idx = -99;
	g.get_best(T0, &idx);

	EXPECT_EQ(idx, 0);
}

// SQE-DVG-MC-13 | DVG-D14 (H true) · DVG-D32/D34 (P,Q,R true, i=0) — real failover
// Given: sensor 0 is current best (d=0, prio 50, call1); call2 re-confirms sensor 0 still wins over fresh sensor 1
//        (prio 40, tie broken by priority, _prev_best stays -1 since no real switch yet); sensor 0 then goes silent
//        past the timeout while sensor 1 stays fresh (call3)
// When : get_best() at call3 (T0+50000); then failover_index()/failover_state()
// Then : idx == 1 at call3 (D14 true via H alone: max_index=1 != _curr_best=0); failover_index() == 0 and
//        failover_state() == TIMEOUT — scanning from i=0: sensor 0 used() true (P), state()==TIMEOUT!=NO_ERROR (Q),
//        i(0)==_prev_best(0) (R) -> returns immediately without reaching sensor 1
TEST_F(SqeDvgMcdcTest, MC13_SensorZeroTimesOutSensorOneTakesOver_RealFailoverReported)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1, val, 0, 40);
	g.get_best(T0 + 1, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 50000, val, 0, 40);
	g.get_best(T0 + 50000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_index(), 0);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_TIMEOUT);
}

// SQE-DVG-MC-14 | DVG-D14 (H false, pair of MC13 on H)
// Given: sensor 0 is current best (d=0, prio 50, call1 seeds _curr_best=0); sensor 1 appears fresh with lower
//        priority 40 — same confidence (1.0), so D13's E&&F path requires F=priority>max_priority which is false
// When : get_best() at call2 (T0+1)
// Then : idx == 0 (D14 false: H=max_index!=_curr_best is False — sensor 0 keeps winning on priority; I is also
//        False since max_confidence stays 1.0, not < FLT_EPSILON)
TEST_F(SqeDvgMcdcTest, MC14_StableSensorKeepsWinningOnPriority_NoFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1, val, 0, 40);
	g.get_best(T0 + 1, &idx);

	EXPECT_EQ(idx, 0);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-18 | DVG-D14 (H false, I false — single-sensor stable control row)
// Given: a DataValidatorGroup(1); sensor 0 selected at T0 (call1), then a second fresh put before the next call
// When : get_best() at call2 (T0+1000), same sensor still best
// Then : idx == 0, failover_count() == 0 — D14 false: H false (max_index stays 0 == _curr_best); I false
//        (confidence stays 1.0, not < FLT_EPSILON, so the AND short-circuits on I without evaluating J)
TEST_F(SqeDvgMcdcTest, MC18_SingleSensorFreshDataStable_NoFailover)
{
	DataValidatorGroup g(1);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(0, T0 + 1000, val, 0, 50);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 0);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-19 | DVG-D14 (I true, J true — pair of MC18 on I, pair of MC20 on J) · DVG-D20T
// Given: continuing MC18's single-sensor group, now silent for longer than the timeout
// When : get_best() at call3, more than 40000us since the last put at T0+1000
// Then : idx == -1, failover_count() == 1, returned pointer non-null (F-02) — D14 true via I&&J (not H): the only
//        sibling times out so confidence()==0 and Loop2 never updates max_index away from the Loop1-reseeded value
//        (0), so max_index==_curr_best==0 -> H false, but max_confidence<FLT_EPSILON (I true) and _curr_best>=0
//        (J true) make the decision true through the second disjunct
TEST_F(SqeDvgMcdcTest, MC19_SingleSensorSilentPastTimeout_FailoverViaConfidenceDrop)
{
	DataValidatorGroup g(1);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(0, T0 + 1000, val, 0, 50);
	g.get_best(T0 + 1000, &idx);
	ASSERT_EQ(idx, 0);

	float *best = g.get_best(T0 + 1000 + 41001, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(g.failover_count(), 1u);
	EXPECT_NE(best, nullptr);
}

// SQE-DVG-MC-20 | DVG-D14 (J false, control row — the only sibling never fed)
// Given: a fresh DataValidatorGroup(1), sensor 0 never put()
// When : get_best(T0, &idx) as the very first call ever
// Then : idx == -1, returned pointer nullptr, failover_count() == 0 — D14 false: max_confidence==0<FLT_EPSILON so
//        I is true, but _curr_best is still its construction value -1 (only ever written at the end of get_best())
//        so J is false; I&&J false. H is also false (max_index stays -1 == _curr_best). This is the control row
//        proving the infeasible vector H=F/I=F/J=F is correctly never produced: here I=T, not I=F.
TEST_F(SqeDvgMcdcTest, MC20_SingleSensorNeverFed_NoCandidateSelected)
{
	DataValidatorGroup g(1);

	int idx = -99;
	float *best = g.get_best(T0, &idx);

	EXPECT_EQ(idx, -1);
	EXPECT_EQ(best, nullptr);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-21 | DVG-D15 (K false, control vector) — O3 structural-only evidence
// Given: a DataValidatorGroup(2); sensor 0 put once (d=50 -> conf .50, prio 50)
// When : get_best(T0, &idx) as the VERY FIRST call ever (_curr_best starts at -1, so Loop1's `i==pre_check_best`
//        never matches and pre_check_prio stays at its init value -1)
// Then : idx == 0, failover_count() == 0 — D14 is True here via H alone (max_index becomes 0, _curr_best was -1,
//        0!=-1), so D15 IS reached and evaluated with K=False (pre_check_prio==-1). Per MCDC_ANALYSIS.md §2, when
//        K=False this call's entry forces the `_curr_best<0` branch at L219 regardless of D15's outcome, so D15's
//        value has NO observable effect on failover_count() or any other getter here — this row is O3 evidence
//        only (structural: line 210, `true_failsafe = false;`, must NOT execute in this test, contrasted with
//        MC01 where it does). The oracle below checks only what IS observable (idx, failover_count); the O3
//        per-test coverage capture is recorded separately (see explain-back).
TEST_F(SqeDvgMcdcTest, MC21_FirstCallKFalseControlVector_NoFailoverCounted)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 50, 50);

	int idx = -99;
	g.get_best(T0, &idx);

	EXPECT_EQ(idx, 0);
	EXPECT_EQ(g.failover_count(), 0u);
}

// SQE-DVG-MC-22 | DVG-D15 (M false, pair of MC01 on M)
// Given: sensor 0 is current best (d=50 -> conf .50, prio 50, call1 seeds pre_check_prio=50/pre_check_confidence=.50);
//        sensor 1 appears with d=5 (conf .95), prio 75 — wins D13 via A&&B (seed confidence .50<0.9f, candidate
//        .95>=0.9f)
// When : get_best() at call2 (T0+1000)
// Then : idx == 1, failover_count() becomes 1 — D15 false via M: pre_check_prio=50 < max_priority=75 (K true,
//        L true) but |pre_check_confidence(.50) - max_confidence(.95)| = 0.45f >= 0.1f (M false), so true_failsafe
//        stays True even though priority also increased (distinguishes M from MC01's small |diff|)
TEST_F(SqeDvgMcdcTest, MC22_HigherPriorityButLargeConfidenceJump_CountsAsFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 50, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1000, val, 5, 75);
	g.get_best(T0 + 1000, &idx);

	EXPECT_EQ(idx, 1);
	EXPECT_EQ(g.failover_count(), 1u);
}

// SQE-DVG-MC-23 | DVG-D32/D34 (Q false, pair of MC13 on Q)
// Given: continuing MC13's post-failover state (sensor 0 timed out, sensor 1 is best); sensor 0 sends fresh data
//        again, which clears its error mask the moment confidence() is called on it (via the next get_best())
// When : failover_index()/failover_state() after sensor 0's mask is reset
// Then : failover_index() == -1, failover_state() == NO_ERROR — at i=0: used() true (P true), but state() is now
//        NO_ERROR (Q false), so R is never evaluated and the loop continues to i=1 where Q is false too (sensor 1
//        never errored), exiting with the function's final return
TEST_F(SqeDvgMcdcTest, MC23_SensorZeroRecoversAfterFailover_NoFailoverReported)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1, val, 0, 40);
	g.get_best(T0 + 1, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 50000, val, 0, 40);
	g.get_best(T0 + 50000, &idx);
	ASSERT_EQ(idx, 1);
	ASSERT_EQ(g.failover_index(), 0);

	g.put(0, T0 + 60000, val, 0, 50);
	g.put(1, T0 + 60000, val, 0, 40);
	g.get_best(T0 + 60000, &idx);

	EXPECT_EQ(g.failover_index(), -1);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_NO_ERROR);
}

// SQE-DVG-MC-24 | DVG-D32/D34 (R false, pair of MC13 on R)
// Given: a DataValidatorGroup(2); the very first get_best() call (call1) has sensor 0 and sensor 1 both fresh with
//        a priority tie (sensor 0 wins on priority) — because _curr_best started <0, the bookkeeping sets
//        _prev_best=max_index=0 directly (not pre_check_best); sensor 1 then goes silent past the timeout while
//        sensor 0 stays fresh and already best, so no new failover happens (_prev_best stays 0)
// When : failover_index()/failover_state() after sensor 1 times out
// Then : failover_index() == -1, failover_state() == NO_ERROR — scanning i=0 (used true, state NO_ERROR -> Q
//        false, continue), i=1 (used true, state TIMEOUT!=NO_ERROR -> Q true, but i(1)==_prev_best(0) is False
//        -> R false) -> loop exits without an early return
TEST_F(SqeDvgMcdcTest, MC24_SensorNeverBestTimesOut_NotReportedAsFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	g.put(1, T0, val, 0, 40);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(0, T0 + 50000, val, 0, 50);
	g.get_best(T0 + 50000, &idx);
	ASSERT_EQ(idx, 0);
	ASSERT_EQ(g.get_sensor_state(1), DataValidator::ERROR_FLAG_TIMEOUT);

	EXPECT_EQ(g.failover_index(), -1);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_NO_ERROR);
}

// SQE-DVG-MC-25 | DVG-D32/D34 (P false, pair of MC13 on P) | code-feasible / domain-invalid (timestamp 0)
// Given: continuing MC13's post-failover state (sensor 0 timed out, _prev_best=0); sensor 0 then receives a fresh
//        put() with the domain-invalid timestamp 0 (no intervening get_best()/confidence() call)
// When : failover_index()/failover_state() right after that put()
// Then : failover_index() == -1, failover_state() == NO_ERROR — put(0,...) unconditionally sets _time_last=0, so
//        used() (`_time_last>0`) becomes False for sensor 0 (P false), short-circuiting Q and R; the loop
//        continues to i=1 where sensor 1's state is NO_ERROR (Q false) -> final return. Code-feasible (put()
//        accepts any timestamp, no validation) but domain-invalid (real hrt_absolute_time() is never exactly 0
//        after boot) — kept as a deliberate probe per MCDC_ANALYSIS.md's code-feasible/domain-valid distinction,
//        not an infeasible-vector exclusion
TEST_F(SqeDvgMcdcTest, MC25_PutWithTimestampZeroResetsUsed_NotReportedAsFailover)
{
	DataValidatorGroup g(2);
	const float val[3] = {1.f, 1.f, 1.f};

	g.put(0, T0, val, 0, 50);
	int idx = -99;
	g.get_best(T0, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 1, val, 0, 40);
	g.get_best(T0 + 1, &idx);
	ASSERT_EQ(idx, 0);

	g.put(1, T0 + 50000, val, 0, 40);
	g.get_best(T0 + 50000, &idx);
	ASSERT_EQ(idx, 1);
	ASSERT_EQ(g.failover_index(), 0);

	g.put(0, 0, val, 0, 50);

	EXPECT_EQ(g.failover_index(), -1);
	EXPECT_EQ(g.failover_state(), DataValidator::ERROR_FLAG_NO_ERROR);
}
