# Candidate matrix (re-verified against the actual v1.17.0 source, P04)

Metrics are `wc -l` and grep heuristics: decisions ≈ `if|while|for|?|case `, compound ≈ lines containing `&&`/`||`.
All numbers below were re-measured directly against `PX4-Autopilot/src` at commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`
(not copied from REF_02) using:
```
grep -rn "px4_add_unit_gtest\|px4_add_functional_gtest" --include=CMakeLists.txt PX4-Autopilot/src   # -> work/scope/upstream_gtests.txt (119 lines)
for f in <file>; do echo "$f lines=$(wc -l <$f) decisions~=$(grep -cE '\bif\b|\bwhile\b|\bfor\b|\?|case ' $f) compound~=$(grep -cE '&&|\|\|' $f)"; done
```

## 1. Candidate matrix

| Candidate | Lines | Decisions~ | Compound~ | Upstream tests? | Dependencies | Verdict |
|---|---|---|---|---|---|---|
| `sensors/data_validator/DataValidator.cpp` | 155 | 17 | 0 | none (`data_validator/CMakeLists.txt` never adds `tests/`; its `test_data_validator.cpp` is dead code — verified, see O-01) | none at runtime (`hrt_absolute_time`/`PX4_INFO_RAW` only inside `print()`) | **Selected (A)** |
| `sensors/data_validator/DataValidatorGroup.cpp` | 347 | 42 | 7 | none (same as above) | `DataValidator` | **Selected (B, MC/DC)** |
| `commander/failure_detector/FailureDetector.cpp` | 354 | 46 | 15 | none | `DEFINE_PARAMETERS` (8 params, confirmed at `FailureDetector.hpp:129-138`), 6 uORB subscriptions (`vehicle_attitude`, `esc_status`, `pwm_input`, `sensor_selection`, `vehicle_imu_status`, `actuator_motors`), `hrt_absolute_time`, `Hysteresis`, `AlphaFilter` | **Selected (C)** |
| `commander/failure_detector/FailureInjector.cpp` | 134 | 17 | 4 | none | `SYS_FAILURE_EN` param (confirmed `FailureInjector.cpp:43`), `vehicle_command` uORB sub | **Selected (D)** |
| `lib/battery/battery.cpp` | 434 | 40 | 15 | none | params, uORB pub, time, filters | stretch candidate — not needed; A–D already give a substantial, coherent scope |
| `land_detector/MulticopterLandDetector.cpp` + `LandDetector.cpp` | 324+259 | 63 | 34 | none | `ModuleBase`/`WorkItem` lifecycle, many topics, protected virtual API | **rejected**: heavy test harness (module start/stop, scheduler), genuinely time-driven state machine better suited to SITL than unit/functional GTest |
| `commander/HealthAndArmingChecks/checks/batteryCheck.cpp` | 324 | 49 | 19 | parent directory has `HealthAndArmingChecksTest` (shared harness) | tightly coupled to the `Context`/`Report` check framework | **rejected**: can't test in isolation without reimplementing framework scaffolding |
| `commander/failsafe/{failsafe,framework}.cpp` | 718+740 | 210 | 97 | `failsafe_test.cpp` exists upstream | large pre-existing state machine | **rejected**: already has upstream coverage; also far too large for the time box (R5 — we don't copy/adapt upstream tests) |
| `manual_control/ManualControlSelector.cpp` | 135 | 16 | 15 | `ManualControlSelectorTest` exists upstream | none | **rejected**: already tested; adding to it wouldn't be a *new* contribution |
| `navigator/GeofenceBreachAvoidance` | 293 | 15 | 2 | yes, upstream test exists | `geo` lib | **rejected**: few compound decisions relative to size, already tested |
| `lib/hysteresis/hysteresis.cpp` | **92 (re-measured)** | **8 (re-measured)** | **2 (re-measured)** | yes — `px4_add_unit_gtest(SRC HysteresisTest.cpp LINKLIBS hysteresis)` confirmed in `lib/hysteresis/CMakeLists.txt:36` | none | **rejected**: already tested upstream; used as a *dependency* inside `FailureDetector` (area C), not a target of its own |
| `commander/Safety.cpp` | **78 (re-measured)** | **5 (re-measured)** | **1 (re-measured)** | none found directly — correction to REF_02: no `Safety`-specific gtest registration exists; nearby commander-dir tests (`ModeManagementTest.cpp`, `mag_calibration_test.cpp`) do not cover it | trivial | **rejected**: too small/trivial a decision surface to be worth a dedicated test suite within the time box |
| `commander/UserModeIntention.cpp` | **102 (re-measured)** | **10 (re-measured)** | **4 (re-measured)** | none found directly (same correction as above) | trivial | **rejected**: same reasoning — small, and arming/mode-intention logic is already exercised indirectly by the (out-of-scope) Commander integration paths |

## 2. Note on re-verification
Every row's metrics above were independently re-measured against the actual source in this clone (not copied from
`docs/reference/REF_02_SCOPE_CANDIDATES.md`) and matched REF_02 exactly for all A–D candidates. Two corrections were made
to REF_02 during re-verification: `Safety.cpp` and `UserModeIntention.cpp` were listed there as having upstream tests
("yes (dir)"); a direct search of `modules/commander/CMakeLists.txt` and the `modules/commander/` tree found no
dedicated gtest registration for either file specifically — the nearby commander-level test files that exist
(`ModeManagementTest.cpp`, `mag_calibration_test.cpp`) do not exercise them. This doesn't change the rejection verdict
(they remain out of scope for being too trivial), but the stated reason is corrected here for accuracy.

## 3. Why A–D together form a substantial, coherent scope
~990 source lines, ~122 decisions, ~26 compound decisions across two safety chains:
- **Sensor redundancy** (A+B): which IMU/magnetometer instance feeds the estimator, and when a failover is declared/reported.
  Confirmed call sites: `modules/sensors/voted_sensors_update.cpp` (`get_best`, `failover_index`, `failover_state`,
  `add_new_validator` — IMU accel/gyro voting) and `modules/sensors/vehicle_magnetometer/VehicleMagnetometer.cpp`
  (same API, magnetometer voting).
- **Failure detection** (C+D): attitude envelope, ESC arming, motor telemetry/under-current, imbalanced propellers,
  external ATS, feeding Commander's failsafe state. Confirmed call site: `modules/commander/Commander.cpp:1857`
  (`_failure_detector.update(...)` drives `vehicle_status.failure_detector_status` and the published `failure_detector_status`
  uORB topic consumed elsewhere in the failsafe chain).

Both chains are pure control/business logic, no GUI/presentation code. Baseline coverage is incidental only
(A/B touched only by the SITL script test `sitl-imu_filtering`; C/D not executed by any existing test) — so the
student-authored suite's contribution is real and measurable, not padding on top of already-covered code.

## 4. Test-level justification (short form for the report)
- **A/B — unit** (`px4_add_unit_gtest`): confirmed no `DEFINE_PARAMETERS`, no uORB subscriptions, no object construction
  reading runtime services in either `DataValidator.cpp` or `DataValidatorGroup.cpp` — the only runtime calls
  (`hrt_absolute_time()`, `PX4_INFO_RAW`) are inside diagnostic `print()` methods, not the decision logic under test.
  Explicit inputs only (timestamps, error counts, priorities passed as arguments). Linked via `modules__sensors` purely to
  resolve platform symbols for `print()`, the same idiom upstream uses for `ManualControlSelectorTest`
  (`LINKLIBS modules__manual_control`) — still a unit test because no runtime service is exercised by the test itself.
- **C/D — functional** (`px4_add_functional_gtest`): confirmed `DEFINE_PARAMETERS` is read at construction
  (`FailureDetector.hpp:129`), `SYS_FAILURE_EN` is read via `param_find`/`param_get` in `FailureInjector.cpp:43`, and
  both subscribe to multiple uORB topics including the multi-instance `vehicle_imu_status`. Hysteresis/timeout logic
  depends on real `hrt_absolute_time()`. This requires `gtest_functional_main`, which initializes uORB + the parameter
  system before the test body runs.
- **Why not SITL for any of A–D**: none of the four files touch drivers, the work-queue scheduler, module start/stop
  lifecycle, or simulator dynamics to reach any of their decisions — SITL would add nondeterminism and wall-clock cost
  without reaching anything the unit/functional levels can't already reach deterministically. (SITL's
  `sitl-imu_filtering` script test already contributes incidental baseline coverage to A/B as a side effect of running
  the sensors module, which we account for separately in the coverage baseline, not as a deliberate test strategy.)
