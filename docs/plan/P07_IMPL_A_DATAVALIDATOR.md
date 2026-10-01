# P07 — Implementation chunk A: `DataValidator` (GTest unit)
**Goal:** tests that execute every statement and both outcomes of every reachable decision in `DataValidator.cpp` with exact oracles.
**Owner:** IMPL. **Gate:** G07. **Spec:** SPEC_02. **Inventory:** REF_03 (decision IDs DV-D01…D15).

## Files
- `PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorTest.cpp` (start from `templates/test_skeletons/SqeDataValidatorTest.cpp`)
- Append to `PX4-Autopilot/src/modules/sensors/data_validator/CMakeLists.txt` (after `px4_add_library(...)`):
  ```cmake
  # SQE A2 (student-authored tests)
  px4_add_unit_gtest(SRC SqeDataValidatorTest.cpp LINKLIBS modules__sensors)
  px4_add_unit_gtest(SRC SqeDataValidatorGroupTest.cpp LINKLIBS modules__sensors)
  ```
  ctest names: `unit-SqeDataValidator`, `unit-SqeDataValidatorGroup`. If linking `modules__sensors` fails, try `LINKLIBS data_validator px4_layer px4_platform`
  and record the outcome in DECISIONS (REF_10 #6). Do **not** add `add_subdirectory(tests)` (legacy tests stay orphaned).

## Build/run loop (fast)
```bash
cd PX4-Autopilot
cmake --build build/px4_sitl_test --target unit-SqeDataValidator 2>&1 | tail -30
(cd build/px4_sitl_test && ./unit-SqeDataValidator --gtest_output=xml:../../../evidence/tests/xml/unit-SqeDataValidator.xml)
```
(First time after adding the CMake line: `make tests TESTFILTER=__no_tests__ PX4_CMAKE_BUILD_TYPE=Coverage` so CMake registers the target.)

## Constants used by the oracles (from DataValidator.hpp, v1.17.0)
timeout default 40000 µs · `NORETURN_ERRCOUNT` 10000 · `ERROR_DENSITY_WINDOW` 100.0f · `VALUE_EQUAL_COUNT_DEFAULT` 100 · flags NO_ERROR 0,
NO_DATA 0x01, STALE 0x02, TIMEOUT 0x04, HIGH_ERRCOUNT 0x08, HIGH_ERRDENSITY 0x10 · confidence = 1 − density/100 when no critical error.
Use `T0 = 1'000'000` µs and steps of 1000 µs unless stated.

## Test catalogue (IDs → gtest names `DVnn_…`; fixture `SqeDataValidatorTest`)
| ID | Scenario (Given → When) | Expected (Then) | Decisions |
|---|---|---|---|
| SQE-DV-01 | fresh; `put(T0,{1,2,3},0,7)` | used; value {1,2,3}; priority 7; error_count 0; `confidence(T0)`=1; state 0; rms {0,0,0} | D01F D02F D03 D04T D05T D07F–D11F D12T D13T |
| SQE-DV-02 | `put(T0,{1,0,0})`, `put(T0+1000,{3,0,0})` | `rms()[0]` = √2 (sample std-dev of deviations {0,2}; EXPECT_NEAR 1e-5); rms[1]=rms[2]=0 | D05F D06T/F |
| SQE-DV-03 | threshold 6; `{5,5,5}` ×3 then ×1 more | after 3 puts count=6 → conf 1, state 0 (6 > 6 is False); after 4th count=9 → conf 0, STALE | D09F/T boundary |
| SQE-DV-04 | threshold 2; `{5,5,5}`×2 (stale) then `{5,5,6}` | first: conf 0 STALE; then count resets at axis 2 → conf 1, state 0 (flags cleared) | D06F D09T/F D13T |
| SQE-DV-05 | default threshold; scalar `put(t,7.f,0,1)` ×34 then ×1 | value[1]=value[2]=0 (padding); conf 1 after 34 (count 99), conf 0 + STALE after 35 (count 102) — header: "accumulated also between axes" (F-04) | D06T D09T/F |
| SQE-DV-06 | `put(T0,{1,2,3})`, `put(T0+1000,{NaN,5,+Inf})` | value {1,5,3} — non-finite axes ignored | D04F |
| SQE-DV-07 | 10× `put(t,{NaN,NaN,NaN},0,1)` | **characterization (F-09):** used; value {0,0,0}; conf 1; state 0 | D04F D05 not reached |
| SQE-DV-08 | `put(T0,v,10)`, `put(T0+1000,v',10)` | conf 0.90 then 0.91 (density 10 → 9) | D01T/F D02T |
| SQE-DV-09 | `put(T0,v,0)`, `put(T0+1000,v',0)` | conf 1 both (density floor 0) | D02F |
| SQE-DV-10 | `put(T0,v,10)`, `put(T0+1000,v',3)` | error_count 3; conf 0.91 (decrease treated as "no new errors") | D01F D02T |
| SQE-DV-11 | fresh; `confidence(T0)` | 0; state NO_DATA | D07T D12F |
| SQE-DV-12 | `put(T0,v)`; conf at T0+40000 and T0+40001; `set_timeout(1000)`; conf at T0+1000, T0+1001 | 1 / 0+TIMEOUT; `get_timeout()`=1000; 1 (TIMEOUT cleared) / 0+TIMEOUT | D08F/T D13T |
| SQE-DV-13 | `put(T0,v,10000)` then `put(T0+1000,v',10001)` | conf 0 with state HIGH_ERRDENSITY (10000 > 10000 False → density branch); then conf 0 with state HIGH_ERRDENSITY\|HIGH_ERRCOUNT | D10F/T D11T D13F D12F |
| SQE-DV-14 | `put(T0,v,101)` then `put(T0+1000,v',101)` | conf 0 + HIGH_ERRDENSITY; then conf ≈ 0.01 and state 0 (proves cap to 100 then −1) | D11T/F D13T |
| SQE-DV-15 | `put(T0,v,100)` | **characterization (F-10):** conf 0 **and** state 0 (no flag at exactly the window) | D11F D12T D13F |
| SQE-DV-16 | `put(T0,v)`; conf(T0+50000); `put(T0+60000,v')`; conf(T0+60000) | 0+TIMEOUT; then 1 and state 0 | D08T D13T |
| SQE-DV-17 | fresh conf(T0); `put(T0+1000,v,10001)`; conf(T0+1000) | state NO_DATA then NO_DATA\|HIGH_ERRCOUNT (flags accumulate while conf stays 0) | D07T D10T |
| SQE-DV-18 | fresh; `print()` with `testing::internal::CaptureStdout()` | output contains "no data" (if capture yields empty, see REF_10 #17) | D14T |
| SQE-DV-19 | `put(1000,v)`; `print()` | **characterization (F-05):** prints 3 lines ("val:"); afterwards state has TIMEOUT (print() calls `confidence(hrt_absolute_time())`) | D14F D15 |
| SQE-DV-20 | `put(0,v)`; `confidence(0)`; `put(1000,v')` | **characterization (F-12):** used()==false, conf 0 + NO_DATA though value()==v; next put re-enters init (rms 0) | D05T D07T |
Probes (DISABLED, human-approved oracles only, run by `tools/sqe_run_tests.sh probes`): `DISABLED_PRB01_NonFiniteStreamShouldReduceConfidence`
(F-09, expects conf < 1 or a flag), `DISABLED_PRB02_DensityAtWindowShouldSetFlag` (F-10, expects state ≠ 0 when conf == 0).

## Done when
All DV tests pass; `tools/sqe_coverage.sh student` shows DataValidator.cpp 100 % lines/branches or GAPS entries; shuffle ×5 stable;
inventory rows added (`work/inventory/test_inventory.csv`). Gate G07.
## Explain-back
Why DV-03 uses threshold 6 (boundary `>`), how DV-05's 35 comes from 3 axes × 34 increments, why DV-14 proves the density cap exists.
