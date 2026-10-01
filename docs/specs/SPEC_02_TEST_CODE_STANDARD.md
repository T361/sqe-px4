# SPEC_02 — Test code standard (binding)

## 1. File header (every Sqe*Test.cpp)
```cpp
/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : <production file(s)>          (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest unit | GTest functional  — justification: <one line>
 * Decisions covered : see test_inventory.csv; each TEST names its decision IDs
 * Authors           : <S1>, <S2>, <S3>   — AI assistance recorded in ai_assistance_log.md
 */
```
Keep the PX4 BSD licence banner **out** of student files (they are not PX4 contributions) unless the team decides otherwise (DECISIONS).

## 2. Per-test comment block (directly above each TEST/TEST_F)
```cpp
// SQE-DVG-MC-04 | DVG-D13 (C,D true) · DVG-D15 (L false) | F-11
// Given: sensor 0 is current best (conf 0.95, prio 50); sensor 1 appears with conf 0.97, prio 50
// When : get_best() at t0+1 ms
// Then : sensor 1 selected (C∧D) and the switch is counted as a failover (L false: 50 < 50 is false)
```

## 3. Structure and oracles
- Given/When/Then (setup → run → check), one behaviour per test; helpers for repeated setup (never hide assertions inside helpers
  except precondition `ASSERT_*`).
- **Exact oracles** from the test basis: return values, getters, published uORB messages, flags/bitmasks, counters. Allowed oracle sources:
  code comments/headers/parameter metadata (e.g. "accumulated also between axes", `@min 0.02`), documented library contracts (AlphaFilter,
  Hysteresis), arithmetic derived independently of the implementation (e.g. Welford sample std-dev), message definitions.
  Forbidden: copying the production expression into the test to compute the expected value (circular oracle); `EXPECT_TRUE(true)`;
  "does not crash" as the only check (except explicit death/smoke tests labelled as such).
- Precondition assertions (`ASSERT_*`) that the intended condition values hold before the action (e.g. `ASSERT_EQ(idx, 0)` after step 1,
  `ASSERT_EQ(g.get_sensor_priority(1), 50)`).
- Characterization tests (assert *actual* behaviour where the specification is silent or ambiguous) must say `characterization (F-nn)` in the
  comment block and are reported as such.
- Probes: `DISABLED_PRBnn_*` assert the *expected* behaviour for a confirmed finding; created only after SPEC_10 steps 1–6 and a human
  approval (DECISIONS). They are executed with `--gtest_also_run_disabled_tests` by `tools/sqe_run_tests.sh probes`.

## 4. Compiler flags you must satisfy (applied globally by PX4)
`-Wall -Wextra -Werror -Wshadow -Wfloat-equal -Wdouble-promotion -Wcast-align -Wlogical-op -Wformat-security -Winit-self -Wpointer-arith
-Wuninitialized -Wunused-variable -Wfatal-errors` (+ GCC: `-fcheck-new`). Consequences:
- Floats: `EXPECT_FLOAT_EQ(a, 0.95f)` / `EXPECT_NEAR(a, 0.95f, 1e-6f)` (upstream idiom: `src/lib/slew_rate/SlewRateTest.cpp`); never `EXPECT_EQ`
  on floats (`-Wfloat-equal`); literals with `f`. If `-Wdouble-promotion` fires in a macro, use `EXPECT_NEAR(static_cast<double>(a), 0.95, 1e-6)`.
- No shadowing of fixture members by locals; no unused variables/parameters; initialise all structs with `{}`.
- No `#pragma GCC diagnostic` to silence warnings; fix the code instead.
- Unsigned vs signed: compare `uint32_t` with `0u`, `UINT32_MAX`; `failover_count()` returns `unsigned`.

## 5. Unit tests (DataValidator / DataValidatorGroup)
- `#include "DataValidator.hpp"` / `"DataValidatorGroup.hpp"` (same directory). Timestamps are plain `uint64_t` µs (`T0 = 1'000'000`).
- Construct fresh objects per test (no static state). `DataValidatorGroup` has no copy — create in the test body.
- Output capture for `print()`: `testing::internal::CaptureStdout(); obj.print(); std::string out = testing::internal::GetCapturedStdout();`
  If PX4's `px4_log_raw` writes to a different `FILE*` and the string is empty, keep the side-effect assertions and log the limitation (REF_10 #17).
- Death tests: fixture name ends with `DeathTest`; `GTEST_FLAG_SET(death_test_style, "threadsafe")` (or `::testing::FLAGS_gtest_death_test_style`
  for older gtest) at the start of the test; `EXPECT_DEATH({ … }, "")`.

## 6. Functional tests (FailureDetector / FailureInjector)
```cpp
class SqeFailureDetectorTest : public ::testing::Test {
protected:
  void SetUp() override {
    param_control_autosave(false);   // avoid autosave busy loop (upstream RCUpdateTest idiom)
    param_reset_all();               // defaults for every test
    ASSERT_GT(hrt_absolute_time(), 1_s);
    _status = {}; _status.arming_state = vehicle_status_s::ARMING_STATE_ARMED;
    _status.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;
    _mode = {};   _mode.flag_control_attitude_enabled = true;
  }
  static void setParamInt(const char *name, int32_t v)  { ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name; }
  static void setParamFloat(const char *name, float v)  { ASSERT_EQ(param_set(param_find(name), &v), PX4_OK) << name; }
  …publications as members: uORB::Publication<vehicle_attitude_s> _att_pub{ORB_ID(vehicle_attitude)}; …
};
```
- Order in every test: set params → construct object under test → publish inputs → `update()` → assert.
- uORB latest-sample semantics: a new subscriber sees the last message ever published on that topic as "updated". Therefore publish your own
  message before the first `update()`, and for "no new message" branches call `update()` twice.
- Multi-instance topics (`vehicle_imu_status`): only in `SqeFailureDetectorImuTest`; publications created once in `SetUpTestSuite()` in a fixed
  order (instance 0, then 1); every test republishes both instances with its own data.
- `vehicle_command` (FailureInjector): publish a neutral command (176) in `SetUp()`; create + drain the `vehicle_command_ack` subscription before acting.
- Time: `using namespace time_literals;`, `std::this_thread::sleep_for(std::chrono::milliseconds(n))` with n ≥ 1.5× the trigger time.
  "Not yet triggered" is asserted right after the `update()` that starts the hysteresis. Never assert that something happened *before* a deadline.

## 7. Independence and repeatability (must be demonstrated)
`tools/sqe_run_tests.sh shuffle` runs each binary with `--gtest_shuffle --gtest_repeat=5` (functional: 10) and a fixed printed seed; any failure is a
BLOCKER until explained. Tests must also pass when run individually (`--gtest_filter=<one test>`).

## 8. CMake registration (the only CMake change allowed)
Append at the end of the existing CMakeLists.txt next to the code under test:
`px4_add_unit_gtest(SRC SqeDataValidatorTest.cpp LINKLIBS modules__sensors)` · `px4_add_functional_gtest(SRC SqeFailureDetectorTest.cpp LINKLIBS modules__commander)`.
Do not change existing lines. Comment line `# SQE A2 (student-authored tests)` above your block.

## 9. Optional allocation-fault technique (GCC only) — `SqeDataValidatorAllocFaultTest.cpp`
Purpose: reach `DataValidatorGroup.cpp` L88 (`if (!validator) return nullptr;`), a documented error path ("@return … nullptr on error") that is
real on NuttX (non-throwing `new`). Separate binary because it replaces global `operator new`:
```cpp
#if defined(__GNUC__) && !defined(__clang__)       // relies on -fcheck-new (GCC only)
static int g_fail_countdown = -1;                   // -1 = disarmed
void *operator new(std::size_t n) { if (g_fail_countdown == 0) { g_fail_countdown = -1; return nullptr; }
  if (g_fail_countdown > 0) { --g_fail_countdown; } void *p = std::malloc(n ? n : 1); if (!p) { throw std::bad_alloc(); } return p; }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
#endif
```
Arm it immediately before the call under test and assert disarmed afterwards. **Pre-verified** on the v1.17.0 sources (GCC 13.3, `-fcheck-new`):
`add_new_validator()` returns `nullptr` and the group keeps one validator (`get_sensor_state(1)` = UINT32_MAX); a failing first allocation in the
constructor yields an empty group (`get_best` → −1/nullptr). Without `-fcheck-new` the constructor runs on a null pointer and the process crashes,
and GCC warns that `operator new` must not return NULL — if you see that warning, you are not on GCC+`-fcheck-new`: do not build this file. Returning null from a throwing `operator new` is outside ISO C++;
state that this is a platform-emulation technique justified by `-fcheck-new` and the NuttX allocator semantics. On Clang: do not build this file
(guard the CMake line with `if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")`) and keep L88 as an environment-limited gap.

## 10. Review checklist (auditor uses it)
ID + decisions in comment · Given/When/Then · exact oracle with named source · preconditions asserted · no circular oracle · warning-free ·
isolation rules (§6) · no timing upper bounds · passes alone and shuffled · inventory row exists.
