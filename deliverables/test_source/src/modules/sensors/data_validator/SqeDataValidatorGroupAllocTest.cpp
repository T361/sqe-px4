/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : DataValidatorGroup.cpp — allocation-failure paths (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest unit, in its own binary — justification: these tests replace the global operator new
 *                      for the whole executable, so they are isolated from every other test binary.
 * Decisions covered : DVG-D05 T (L88 `!validator`), the compiler-emitted null checks after `new` at L55 and L86
 *                      (gap G-01), and finding F-13 (allocation failure inside the constructor)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 *
 * Why this is a faithful environment and not a production change: PX4 compiles all C++ with -fcheck-new
 * (cmake/px4_add_common_flags.cmake), because on NuttX flight targets operator new returns nullptr on exhaustion
 * instead of throwing. On the POSIX test host, operator new throws, so the null checks are dead code there. The
 * replacement below reproduces the NuttX behaviour for a chosen allocation only; every other allocation is a plain
 * malloc-backed new that throws std::bad_alloc on failure as usual.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <new>

#include "DataValidatorGroup.hpp"

namespace
{
// 0 = never fail. N > 0 = the N-th operator new call from now returns nullptr (NuttX semantics); later calls succeed.
int g_fail_on_call = 0;
}

void *operator new(std::size_t size)
{
	if (g_fail_on_call > 0 && --g_fail_on_call == 0) {
		return nullptr;
	}

	void *p = std::malloc(size ? size : 1);

	if (p == nullptr) {
		throw std::bad_alloc();
	}

	return p;
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

namespace
{
constexpr uint64_t T0 = 1'000'000;

// Arms the hook for the n-th next allocation and disarms it again when the scope ends, so a test that fails half-way
// can never leave the hook armed for the next test.
class FailNthAllocation
{
public:
	explicit FailNthAllocation(int n) { g_fail_on_call = n; }
	~FailNthAllocation() { g_fail_on_call = 0; }
};
}

class SqeDvgAllocTest : public ::testing::Test
{
protected:
	void TearDown() override { g_fail_on_call = 0; }
};

// SQE-DVG-AF-01 | DVG-D05 T · G-01 (L86 null check, L88 True, L89)
// Given: a DataValidatorGroup(1) with one working sensor (sensor 0 fed, selected by get_best())
// When : add_new_validator() is called while the next allocation returns nullptr (NuttX out-of-memory behaviour)
// Then : add_new_validator() returns nullptr (L88-89) without touching _last; the group still has exactly one
//        sibling (index 1 not found) and still selects sensor 0 — the failed allocation left it intact
TEST_F(SqeDvgAllocTest, AF01_AddNewValidatorAllocationFails_ReturnsNullAndGroupUnchanged)
{
	DataValidatorGroup g(1);
	const float val[3] = {1.f, 1.f, 1.f};
	g.put(0, T0, val, 0, 50);

	DataValidator *added = nullptr;
	{
		FailNthAllocation fail(1);
		added = g.add_new_validator();
	}

	EXPECT_EQ(added, nullptr);
	EXPECT_EQ(g.get_sensor_state(1), UINT32_MAX);

	int idx = -99;
	g.get_best(T0, &idx);
	EXPECT_EQ(idx, 0);

	// control: the same call succeeds once allocation works again, so the nullptr above came from the failed new
	DataValidator *added_ok = g.add_new_validator();
	EXPECT_NE(added_ok, nullptr);
	EXPECT_NE(g.get_sensor_state(1), UINT32_MAX);
}

// SQE-DVG-AF-02 | G-01 (L55 null result, constructor) · DVG-D02 T · DVG-D03 F
// Given: the only allocation in DataValidatorGroup(1)'s constructor returns nullptr
// When : the group is constructed and queried
// Then : construction does not crash (i==0 stores nullptr in _first, the loop ends, `if (_first)` is False); the
//        group behaves as empty: get_sensor_state(0)==UINT32_MAX, get_best() returns nullptr with index -1
TEST_F(SqeDvgAllocTest, AF02_ConstructorSingleAllocationFails_GroupIsEmptyNoCrash)
{
	FailNthAllocation fail(1);
	DataValidatorGroup g(1);

	EXPECT_EQ(g.get_sensor_state(0), UINT32_MAX);

	int idx = -99;
	float *best = g.get_best(T0, &idx);
	EXPECT_EQ(best, nullptr);
	EXPECT_EQ(idx, -1);
}

// SQE-DVG-AF-03 | F-13 (runtime evidence) · DVG-D02 F
// Given: DataValidatorGroup(3) where the 2nd of its 3 allocations returns nullptr
// When : the constructor runs
// Then : the process crashes: at i=1 `prev = next` sets prev to nullptr, so at i=2 `prev->setSibling(next)` (L61)
//        dereferences null. This is F-13: the constructor does not handle a failed allocation of a middle sibling.
//        It can only happen with NuttX's non-throwing new under memory exhaustion at start-up.
TEST_F(SqeDvgAllocTest, AF03_ConstructorMiddleAllocationFails_CrashesOnNullPrev)
{
	GTEST_FLAG_SET(death_test_style, "threadsafe");

	EXPECT_DEATH({
		FailNthAllocation fail(2);
		DataValidatorGroup g(3);
	}, "");
}
