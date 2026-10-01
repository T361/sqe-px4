# SCOPE_RECORD — SE3002 A2, PX4-Autopilot v1.17.0 @ d6f12ad1c4f70ad3230afd7d86e971421e02fef4

Scope re-verified against the live source in P04 (see `work/scope/CANDIDATES.md` for the full candidate matrix and
rejection reasons). All four areas below have **zero upstream GTest coverage** (`work/scope/upstream_gtests.txt`,
119 registrations total, none matching `data_validator` or `failure_detector`).

Baseline coverage percentages are filled in once P03 (coverage pipeline) completes — not yet run as of this write-up
(P02 baseline build still in progress). Marked `PENDING-P03` below rather than guessed, per R3.

---

### Area A — `src/modules/sensors/data_validator/DataValidator.cpp` (`DataValidator`)
**Responsibility / critical behaviour:** tracks one sensor instance's recent value stream and derives a health
"confidence" (0.0–1.0) from timeout, staleness (value frozen vs. last sample), and error count/density, used by the
voter (Area B) to rank and select sensors.
**Key dependencies & state:** no params, no uORB. Internal state carried between calls: rolling value buffer, error
counters, `_time_last_*` timestamps. Only external input is `hrt_absolute_time()`-style timestamps passed explicitly
as arguments (not read live), plus `put()`/`confidence()` call sequences driven entirely by the caller (Area B or a
test).
**Why non-trivial / why included:** 155 lines, 17 decision points, 0 compound (every branch is a single condition —
this is exactly why MC/DC is argued for Area B, not A: A has no compound decisions to derive independence pairs from).
Feeds directly into the voting logic that selects which physical sensor's data the EKF uses.
**PX4 test level + justification:** unit (`px4_add_unit_gtest`). No `DEFINE_PARAMETERS`, no uORB subscription, object
construction reads no runtime service. The only runtime call (`hrt_absolute_time()`/`PX4_INFO_RAW`) is inside the
diagnostic `print()` method, not the decision logic. Link against `modules__sensors` only to resolve `print()`'s
platform symbols, same idiom as upstream's `ManualControlSelectorTest`.
**Exclusions inside the file:** `print()` kept in scope for statement/branch coverage (it does contain branches — the
per-axis formatting loop) but excluded from MC/DC derivation as it is diagnostic output, not business logic (D-004).
**Baseline coverage:** PENDING-P03.

---

### Area B — `src/modules/sensors/data_validator/DataValidatorGroup.cpp` (`DataValidatorGroup`) — **MC/DC component**
**Responsibility / critical behaviour:** owns a linked list of `DataValidator` instances (one per physical sensor
instance), ranks them by confidence × priority, selects the "best" sensor index, and classifies/announces failover
events when the best sensor changes.
**Key dependencies & state:** depends on Area A (`DataValidator`). Internal state: linked list of validators,
`_curr_best`/`_prev_best` indices, per-sensor priority array. No params, no uORB directly (the *caller*, e.g.
`voted_sensors_update.cpp`, owns the uORB side).
**Why non-trivial / why included:** 347 lines, 42 decision points, **7 compound decisions** (`&&`/`||`) — this is
the file with enough logical complexity to justify MC/DC. Confirmed safety-critical call sites:
`modules/sensors/voted_sensors_update.cpp` (`get_best`, `failover_index`, `failover_state`, `add_new_validator` for
IMU accel/gyro voting) and `modules/sensors/vehicle_magnetometer/VehicleMagnetometer.cpp` (same API for magnetometer
voting). This is literally the logic that decides which IMU/mag instance feeds the state estimator and when a
failover is declared.
**PX4 test level + justification:** unit (`px4_add_unit_gtest`), same reasoning as Area A — no params/uORB/construction
side effects, explicit-input pure logic. **MC/DC justification:** chosen over A/C/D because it has both (a) the
highest compound-decision density per line among the unit-level candidates and (b) the clearest safety argument
(sensor selection directly determines estimator input) — argued fully in `docs/plan/P06_MCDC_DERIVATION.md` /
`work/mcdc/MCDC_ANALYSIS.md` once P06 runs.
**Exclusions inside the file:** `print()` (diagnostic dump of validator states) kept for statement/branch, excluded
from MC/DC (D-004) — same reasoning as Area A.
**Baseline coverage:** PENDING-P03.

---

### Area C — `src/modules/commander/failure_detector/FailureDetector.cpp` (`FailureDetector`)
**Responsibility / critical behaviour:** detects in-flight failures — attitude envelope exceedance (roll/pitch +
trigger time), ESC arming/telemetry faults, motor under-current, imbalanced-propeller vibration metric, and external
auto-trigger-system (ATS) signals — and aggregates them into a status bitmask that feeds Commander's failsafe
decision.
**Key dependencies & state:** `DEFINE_PARAMETERS` with 8 params read at construction (`FailureDetector.hpp:129-138`:
`FD_FAIL_P`, `FD_FAIL_R`, `FD_FAIL_R_TTRI`, `FD_FAIL_P_TTRI`, `FD_EXT_ATS_EN`, `FD_EXT_ATS_TRIG`, `FD_ESCS_EN`,
`FD_IMB_PROP_THR`); 6 uORB subscriptions (`vehicle_attitude`, `esc_status`, `pwm_input`, `sensor_selection`,
`vehicle_imu_status` — multi-instance, `actuator_motors`); real `hrt_absolute_time()`-driven `Hysteresis` and
`AlphaFilter` state for debouncing.
**Why non-trivial / why included:** 354 lines, 46 decision points, **15 compound decisions** — the largest and most
logically dense of the four areas. Confirmed call site: `modules/commander/Commander.cpp:1857`
(`_failure_detector.update(_vehicle_status, _vehicle_control_mode)`), whose result is published on the
`failure_detector_status` uORB topic and feeds Commander's failsafe state machine.
**PX4 test level + justification:** functional (`px4_add_functional_gtest`). Params are read at construction, so the
test must set params *before* constructing the object under test (see `docs/specs/SPEC_02` setup rules). uORB inputs
must be published before the first `update()` call. Requires `gtest_functional_main` to initialize uORB + param
system.
**Exclusions inside the file:** none beyond the general diagnostic-logging exclusion; all 15 compound decisions here
are real business logic (failure thresholds), not candidates for MC/DC exclusion — but MC/DC itself is scoped to
Area B only (D-003), per the assignment's "one justified safety-critical component" requirement; C is covered to
decision/branch level, not MC/DC.
**Baseline coverage:** PENDING-P03.

---

### Area D — `src/modules/commander/failure_detector/FailureInjector.cpp` (`FailureInjector`)
**Responsibility / critical behaviour:** reads the `SYS_FAILURE_EN` parameter and injected `vehicle_command`s to
simulate motor failures for in-the-loop failure-detector testing; this is itself a test/validation tool shipped in
production code, and its injected failure state is consumed by `FailureDetector`'s control path.
**Key dependencies & state:** `SYS_FAILURE_EN` read via `param_find`/`param_get` (not `DEFINE_PARAMETERS`, a
different idiom — confirmed `FailureInjector.cpp:43`); subscribes to `vehicle_command` uORB topic for injection
commands and publishes acks.
**Why non-trivial / why included:** 134 lines, 17 decision points, 4 compound decisions. Small but directly in the
control path of Area C (`FailureDetector` reads the injected failure mask), and its own command-parsing/ack logic has
real decision structure worth exercising.
**PX4 test level + justification:** functional (`px4_add_functional_gtest`), same reasoning as Area C — param read
at construction/first-use, uORB command subscription, needs `gtest_functional_main`.
**Exclusions inside the file:** none.
**Baseline coverage:** PENDING-P03.

---

## Exclusions (file-spanning, D-004 — HUMAN-DECISION, flagged for team review)
- Header-only pure accessors across all four files' `.hpp` (`sibling()`, `priority()`, `state()`, `failover_count()`,
  `getStatus()`, `getStatusFlags()`, `getImbalancedPropMetric()`, `getMotorFailures()`, `getMotorStopMask()`, …):
  excluded because they are trivial single-statement returns with no branches — including them would inflate the test
  count without testing any decision logic.
- `src/modules/sensors/data_validator/tests/` (legacy `test_data_validator.cpp`, `test_data_validator_group.cpp`):
  confirmed orphaned — `data_validator/CMakeLists.txt` builds only `DataValidator.cpp`/`DataValidatorGroup.cpp` into
  the `data_validator` library and never references `tests/` (verified by reading the CMakeLists.txt directly;
  recorded as observation O-01). Not built, not run, not copied — read only to learn naming conventions (R5).
- `print()` diagnostic-dump methods in Areas A and B: kept in scope for statement/branch coverage (they have real
  branches) but excluded from the MC/DC derivation in Area B, since they are diagnostic output, not control/business
  logic the component's safety behaviour depends on.

**This exclusions list is a HUMAN-DECISION item (D-004) per CLAUDE.md R7/R10** — the team should review it before
treating G04 as substantively approved, even though the gate row is marked APPROVED here under the autonomous-run
authorization. None of these exclusions hide hard business logic; all are either dead code, pure accessors, or
clearly-diagnostic output.

## O-01 — orphaned legacy tests (observation)
`grep -c ecl_tests_data_validator work/scope/upstream_gtests.txt` → 0 (confirmed: the string doesn't appear at all,
since no `px4_add_unit_gtest`/`px4_add_functional_gtest` registration references it). `data_validator/CMakeLists.txt`
confirmed to define only `px4_add_library(data_validator DataValidator.cpp DataValidator.hpp DataValidatorGroup.cpp
DataValidatorGroup.hpp)` — no `add_subdirectory(tests)`, no test target. The `tests/` directory and its CMakeLists.txt
are present in the source tree but never invoked by the build. This is why R5 singles this directory out: it looks
like a ready-made test suite but is actually dead, unbuilt code referencing a nonexistent `ecl_validation` target.
