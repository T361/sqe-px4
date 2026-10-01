# FINDINGS (format SPEC_10 §2). Start from docs/reference/REF_07_FINDINGS_REGISTER.md; every entry needs a verdict + evidence.

Walked all 15 REF_07 candidates (F-01..F-15) this session (P11, 2026-10-01). Every verdict below was produced by
(a) reading the live production source at the cited file:line this session, (b) reading the actual student test
code and its assertions, and (c) for F-01/F-02/F-03/F-07a/F-07b/F-08/F-11/F-12/F-15 also re-running the relevant
ctest/probe binary this session and inspecting the XML result. None of REF_07's provisional classifications were
trusted without this independent check (R3/R4). Zero un-investigated non-PASS results remain: `python3
tools/sqe_trace_check.py --results evidence/tests/xml/*.xml` reports 116 results, 5 not PASS, and all 5 are the
by-design `DISABLED_PRBnn` probes (not failures requiring SPEC_10's 6-step protocol -- they are deliberately
disabled pending human oracle approval, a separate, already-satisfied workflow: each has a header comment stating
the expected-vs-actual disagreement and citing the finding it probes). Independent re-run this session:
`ctest -R Sqe` -> 5/5 binaries, 100% passed (`evidence/tests/logs/P11_ctest_confirm.log`).

Verdict counts: 13 confirmed, 0 rejected, 1 latent/unverified-at-runtime (F-13), 0 outright false.
Of the 13 confirmed, 3 are explicitly flagged HUMAN-DECISION (F-09, F-10, F-11) because the correct oracle is a
specification question, not something this session can resolve unilaterally (R4, R7). F-07a/F-07b are confirmed by
static source analysis only; no sanitizer build was attempted this session (see "Sanitizer evidence" note below).

---

### F-01 `best != nullptr` False branch is infeasible                 Status: confirmed
Location: DataValidatorGroup.cpp:213 (`if (best != nullptr) { best->reset_state(); }`)
Related tests: structural gap, not a test -- proof-based
Evidence: `work/basis/CFG_DataValidatorGroup_get_best.md` (DVG-D16 sketch), `work/mcdc/MCDC_ANALYSIS.md`,
`work/GAPS.md` G-03 (full re-derivation this session, cross-checked against live source read 2026-10-01)
Reproduction: n/a -- proof by control-flow dominance, not execution
Expected: the False branch of L213 would require `best == nullptr` while the enclosing `if` (DVG-D15, requires
`pre_check_prio != -1`) is reached. Actual: read `DataValidatorGroup.cpp` lines 146-213 this session -- `pre_check_prio`
is reassigned only inside the Loop-1 seed block (L158-169), the exact same unconditional block that assigns
`best = next` (L168, with `next` provably non-null from the `while (next != nullptr)` guard at L157). There is no
path that sets `pre_check_prio != -1` without `best` also being non-null, so L213's False side cannot be reached
without a production-code change (out of R2 scope).
Classification + rationale: structural observation / infeasible, per the control-flow proof -- not a test gap.
Severity / reachability: none -- not a behavioural finding, a tool-coverage artefact of a mathematically redundant
guard.
Report text: `DataValidatorGroup.cpp:213`'s `best != nullptr` null-check is dead code under all reachable inputs:
the only code path that sets the preceding condition's first operand true always also assigns `best` a non-null
pointer in the same unconditional block. Confirmed by tracing every assignment to `pre_check_prio`/`best` this
session; no production-code change was made to simplify it (R2).

---

### F-02 Total failure returns non-null pointer with `index = -1`     Status: confirmed
Location: DataValidatorGroup.cpp:234-245 (specifically L244-245, `*index = max_index; return (best) ? best->value() : nullptr;`)
Related tests: SQE-DVG-11 (`DVG11_BothSensorsTimeOutAfterOneWasBest_IndexNegativeButPointerNonNull`),
SQE-DVG-MC-19 (`MC19_SingleSensorSilentPastTimeout_FailoverViaConfidenceDrop`), SQE-PRB-03 (disabled probe)
Evidence: `evidence/tests/xml/unit-SqeDataValidatorGroup.xml` (DVG11, MC19 both PASS this session via `ctest -R Sqe`),
`evidence/tests/probes/unit-SqeDataValidatorGroup.xml` (PRB03 FAILS as designed, re-run this session)
Reproduction: `DataValidatorGroup g(2); g.put(0,T0,val,0,50); g.get_best(T0,&idx)` (idx==0) -> `g.get_best(T0+50000,&idx)`
after both sensors time out -> `idx==-1`, `get_best()`'s return value is non-null (`EXPECT_NE(best, nullptr)` in
DVG11, confirmed PASS).
Expected (per a stricter reading of the header comment "A data validation group... pointer to the array of best
values"): nullptr once nothing is actually selected. Actual: a non-null pointer to the last-known values of the
formerly-best (now-failed) sensor, with `*index == -1` as the only signal that selection failed.
Classification + rationale: API-inconsistency finding (candidate defect vs. header documentation), confirmed real
by execution (DVG11/MC19 pass with this exact assertion; PRB03 -- which asserts the stricter nullptr reading --
fails, proving the two readings are genuinely mutually exclusive, not a wording ambiguity that happens to coincide).
Severity / reachability: read both call sites this session: `voted_sensors_update.cpp:195-196`
(`_accel.voter.get_best(time_now_us, &accel_best_index); _gyro.voter.get_best(time_now_us, &gyro_best_index);` --
return value discarded, not even assigned) and `VehicleMagnetometer.cpp:536`
(`_voter.get_best(time_now_us, &best_index);` -- same pattern, return value discarded). Both production callers use
only the `*index` out-parameter and never dereference the returned pointer, so this is currently latent, not
exploited -- but it is a genuine landmine for any future caller that trusts the header's phrasing.
Correction to REF_07: REF_07 cites "MC18" as evidence for F-02; this session's reading of the actual test code
shows MC18 is a stable single-sensor no-failover control row (`MC18_SingleSensorFreshDataStable_NoFailover`)
that never exercises the total-failure path at all. The test that actually demonstrates F-02's idx=-1/non-null
return is MC19 (`MC19_SingleSensorSilentPastTimeout_FailoverViaConfidenceDrop`), not MC18 -- this is noted here
as a correction to the register, not a hidden substitution; the finding itself is unaffected.
Report text: `get_best()` returns the previously-best sensor's stale value pointer (not nullptr) even when
`*index == -1` signals total failure, contradicting the header's "array of best values" phrasing. Both production
callers discard the return value and consume only `*index`, so impact is latent today but the API is misleading for
future callers.

---

### F-03 `DataValidatorGroup(0)` + `add_new_validator()` null-pointer dereference   Status: confirmed
Location: DataValidatorGroup.cpp:67 (`_last = next;`, stays nullptr when `siblings==0`), L92 (`_last->setSibling(validator)`)
Related tests: SQE-DVG-10 (`DVG10_AddNewValidatorOnEmptyGroup_CrashesOnNullLast`, death test)
Evidence: `evidence/tests/xml/unit-SqeDataValidatorGroup.xml` (DVG10 PASS -- `EXPECT_DEATH` on SIGSEGV, confirmed
this session via `ctest -R Sqe`)
Reproduction: `DataValidatorGroup g(0); g.add_new_validator();` -> SIGSEGV via `GTEST_FLAG_SET(death_test_style,
"threadsafe"); EXPECT_DEATH({...}, "")`.
Expected: either defined/documented behaviour for the zero-sibling case, or a precondition stated in the header.
Actual: unconditional crash -- `add_new_validator()` at L92 dereferences `_last` with no null check.
Classification + rationale: latent robustness defect (undocumented precondition) -- confirmed by direct execution.
Severity / reachability: grepped all production `DataValidatorGroup` construction sites this session
(`grep -rn "voter{" src/modules/sensors/`): `voted_sensors_update.h:114` (`DataValidatorGroup voter{1};`),
`VehicleAirData.hpp:121` (`DataValidatorGroup _voter{1};`), `VehicleMagnetometer.hpp:157`
(`DataValidatorGroup _voter{1};`) -- every production call site uses `{1}`, never `{0}`. Unreachable from
production as REF_07 claims; only reachable via direct unit-level misuse (exactly what DVG10 exercises).
Report text: `DataValidatorGroup(0)` followed by `add_new_validator()` dereferences a null `_last` pointer and
crashes (confirmed SIGSEGV via `EXPECT_DEATH`). All three production construction sites use `{1}`, so this is
unreachable in the shipped product, but the public API offers no protection against misuse if a future caller
passes 0.

---

### F-04 Stale-value counter accumulates across padded/zero axes      Status: confirmed
Location: DataValidator.cpp:82-87 (`_value_equal_count` comparison is per-axis inside the dimensions loop, but a
single shared counter)
Related tests: SQE-DV-05 (`DV05_ScalarPut_PadsAxesAndSharesEqualCountAcrossAxes`)
Evidence: `evidence/tests/xml/unit-SqeDataValidator.xml` (DV05 PASS this session)
Reproduction: 34x `put(t, 7.f, 0, 1)` (scalar put pads axes 1/2 with 0.0f, which trivially stay equal every call) ->
35th put reaches `_value_equal_count_threshold` -- because all 3 axes' equal-counts share one counter -> `confidence()`
reports `ERROR_FLAG_STALE_DATA` far sooner than a genuinely varying single-axis signal would. Confirmed via DV05's
assertions (`state() == 0` at 34 puts, `STALE_DATA` at 35th).
Expected: documented in the header as a specification trade-off (single shared counter across dimensions), not a
crash or silent wrong-answer.
Classification + rationale: specification ambiguity (REF_07's own classification, confirmed consistent with reading
the loop structure at L67-88: `_value_equal_count` is incremented/reset once per axis iteration inside the shared
`for` loop, so padded zero axes contribute "equal" votes on every call regardless of the real (non-padded) axis's
actual variability).
Severity / reachability: confirmed `sensors.cpp:285` this session --
`_airspeed_validator.put(diff_pres.timestamp_sample, airspeed_input, diff_pres.error_count, 100);` where
`airspeed_input[3] { diff_pres.differential_pressure_pa, 0.0f, 0.0f }` is exactly the padded single-channel pattern.
A real, constant-ish airspeed signal padded this way reaches the stale-data threshold roughly 3x faster than a true
3-axis constant signal would, because the two zero-padded axes also "vote" equal every cycle.
Report text: `DataValidator`'s stale-value counter is shared across all 3 axes rather than tracked per-axis. A
scalar sensor (e.g. airspeed, `sensors.cpp:285`, which pads unused axes with 0.0f) reaches the "stale" threshold
after fewer real samples than a genuinely 3-axis sensor would, because the padded axes contribute equal-value votes
every cycle. Confirmed via DV05's direct execution; documented in the header as the accepted design trade-off.

---

### F-05 `print()` mutates the error mask via `confidence()` side effect   Status: confirmed
Location: DataValidator.cpp:152-153 (`confidence(hrt_absolute_time())` called inside `print()`'s `PX4_INFO_RAW` args)
Related tests: SQE-DV-19 (`DV19_PrintWithData_PrintsThreeLinesAndTimesOutAsSideEffect`)
Evidence: `evidence/tests/xml/unit-SqeDataValidator.xml` (DV19 PASS this session)
Reproduction: `put(1000, val, 0, 0)` then `print()` (no explicit `confidence()` call by the test) -> `state() ==
ERROR_FLAG_TIMEOUT` afterward, proving `print()` itself set the flag via its internal `confidence()` call.
Expected: a diagnostic/print function should be read-only. Actual: `print()` calls `confidence(hrt_absolute_time())`
once per axis (3x), which -- because real wall-clock time is far past the `put()` timestamp (1000us) in the test --
triggers the `ERROR_FLAG_TIMEOUT` branch and mutates `_error_mask` as a side effect of printing.
Classification + rationale: diagnostic side effect, confirmed by execution -- low severity.
Severity / reachability: `sensors status` (the operator-facing diagnostic command that calls `print()`) can set
TIMEOUT flags on an otherwise-untouched/stale validator purely by being invoked, which could confuse a subsequent
read of `state()` by another part of the system expecting print to be read-only.
Report text: `DataValidator::print()` is not read-only: it calls `confidence(hrt_absolute_time())`, which can set
`ERROR_FLAG_TIMEOUT` on the validator purely as a side effect of being printed. Confirmed via DV19: after only
`put()` + `print()` (no explicit `confidence()` call), `state()` reports `TIMEOUT`.

---

### F-06 1% confidence quantisation is optimisation-level-sensitive   Status: confirmed
Location: DataValidatorGroup.cpp:189 (`fabsf(confidence - max_confidence) < 0.01f`), DataValidator.cpp:134
(`ret = 1.0f - (_error_density / ERROR_DENSITY_WINDOW)`)
Related tests: none directly in the submitted suite -- the matrix deliberately avoids test vectors that sit on this
boundary (see below); the phenomenon itself is confirmed by direct float32 arithmetic in `work/mcdc/MCDC_ANALYSIS.md`
Evidence: `work/mcdc/MCDC_ANALYSIS.md` section 4 ("Float conditions"), re-derived this session: cross-checked the
arithmetic by hand against the cited formula -- `1.0f - 10/100.0f` in IEEE-754 float32 is `0.89999998...f`, strictly
less than the nominal `0.9f` `MIN_REGULAR_CONFIDENCE` threshold, confirming the quantisation/rounding sensitivity
REF_07 describes exists in the real formula.
Reproduction: n/a (characterised via arithmetic, not a dedicated failing test) -- REF_07's own pre-verification
(different `-O0` vs `-O2 -freciprocal-math` adjacent-step outcome sets) is cited as evidence-transfer basis and was
not independently re-run under a second optimisation level this session (would require a second build).
Expected/Actual: the same nominal `d` (error-density step count) can evaluate the E condition (`fabsf(...) <
0.01f`) differently depending on optimisation level/FP contraction, because the quantity being compared is itself
the result of float32 division and subtraction with no guaranteed exact representation at 1% steps.
Classification + rationale: specification ambiguity / evidence-transfer limitation -- `work/mcdc/MCDC_ANALYSIS.md`
confirms the matrix's own 12 chosen D13 test vectors deliberately stay >=0.01-0.02 away from this boundary so the
submitted suite's PASS/FAIL results are not sensitive to it, but the underlying code's boundary behaviour is
real and optimisation-dependent.
Severity / reachability: the measured (`-O0`, this project's Coverage build) and an optimised flight build
(`-O2`/`-O3`) could in principle pick different "best" sensors for the same nominal inputs if a real-world signal's
error density happens to land within ~1 count of the 1% boundary -- not exercised by the submitted suite, flagged as
a characteristic of the algorithm, not a test gap.
Report text: The 1%-equal-confidence tie-break (`DataValidatorGroup.cpp:189`) compares float32 values derived from
integer error-density counts; the exact outcome for adjacent density steps can differ between optimisation levels
(verified by direct float32 arithmetic: `d=10` -> `0.89999998f`, not exactly `0.9f`). The submitted MC/DC test
vectors were deliberately chosen to avoid this boundary so test results are stable, but the production algorithm's
tie-break is inherently float-rounding-sensitive at the 1% step.

---

### F-07a Unclamped `esc_count` loop in `manipulateEscStatus`        Status: confirmed (static analysis only)
Location: FailureInjector.cpp:117 (`for (int i = 0; i < status.esc_count; i++)`) -- no `CONNECTED_ESC_MAX` clamp,
unlike the sibling loop at L67 (`for (int i = 0; i < esc_status_s::CONNECTED_ESC_MAX; i++)`)
Related tests: SQE-PRB-05 (`DISABLED_PRB05_EscCountAboveMax`)
Evidence: `evidence/tests/probes/functional-SqeFailureInjector_PRB0506.xml` (PRB05 runs and reports OK/PASSED
this session in the normal Coverage build -- it is an inert `SUCCEED()` placeholder, not a real detection)
Reproduction: `esc.esc_count = 9` (exceeds the fixed `esc[8]` array bound) -> `manipulateEscStatus(esc)` writes past
the array in a debug/ASan build; in this session's normal (non-sanitizer) build the out-of-bounds write is
undefined behaviour that happens not to crash, and the test's body ends in `SUCCEED()` by design (never asserts
anything that could fail without a sanitizer).
Expected: a bounds clamp consistent with the sibling loop at L67 (which is bounded to `CONNECTED_ESC_MAX`). Actual:
confirmed via direct source read this session -- `FailureInjector.cpp:117` has no such clamp.
Classification + rationale: robustness defect (out-of-contract input), confirmed by source analysis; not
confirmed by runtime detection this session (no ASan build was performed -- see "Sanitizer evidence" note below).
Severity / reachability: only reachable with failure injection active (`SYS_FAILURE_EN=1`) and an `esc_status`
message whose `esc_count` exceeds 8 -- requires a specific, fault-model-controlled uORB publish, not reachable
through normal flight-code paths.
Report text: `FailureInjector::manipulateEscStatus()` loops to `status.esc_count` with no `CONNECTED_ESC_MAX` clamp
(confirmed absent by reading `FailureInjector.cpp:117` this session), unlike the sibling loop at L67. An
`esc_status` with `esc_count > 8` would write past the fixed `esc[8]` array. Confirmed via static source analysis
only; PRB05 is an inert placeholder in this session's non-sanitizer build (`SUCCEED()`, no real detection) -- a
dedicated ASan rebuild (not performed this session, see rationale below) would be needed to observe the fault at
runtime.

---

### F-07b Unguarded shift in `manipulateEscStatus` for non-motor actuator functions   Status: confirmed (static analysis only)
Location: FailureInjector.cpp:118-126 (`const unsigned i_esc = status.esc[i].actuator_function -
actuator_motors_s::ACTUATOR_FUNCTION_MOTOR1;` then `1 << i_esc` at L120/L126, no range guard), contrast
FailureDetector.cpp:274 (`if (i_esc >= actuator_motors_s::NUM_CONTROLS) { continue; }` -- the guard this code lacks)
Related tests: SQE-PRB-06 (`DISABLED_PRB06_NonMotorFunctionShift`)
Evidence: `evidence/tests/probes/functional-SqeFailureInjector_PRB0506.xml` (PRB06 runs and reports OK/PASSED
this session -- inert `SUCCEED()` placeholder, same as PRB05)
Reproduction: `esc.esc[0].actuator_function = 0` (not `ACTUATOR_FUNCTION_MOTOR1`-based) -> `i_esc` underflows to a
huge unsigned value -> `1 << i_esc` is an out-of-range shift exponent (UB in C++) in a normal build; not detected
without UBSan.
Expected: a range guard consistent with `FailureDetector.cpp:274`'s `i_esc >= NUM_CONTROLS` check. Actual:
confirmed via direct source read this session -- no equivalent guard exists in `FailureInjector.cpp`.
Classification + rationale: undefined behaviour, confirmed by source analysis; not confirmed by runtime detection
this session.
Severity / reachability: only reachable with failure injection active and an `esc_status` entry whose
`actuator_function` is not motor-derived -- same injection-only reachability constraint as F-07a.
Report text: `FailureInjector::manipulateEscStatus()` derives `i_esc` by subtracting `ACTUATOR_FUNCTION_MOTOR1`
with no range check before using it as a shift exponent (`1 << i_esc`), unlike `FailureDetector.cpp:274`'s explicit
`i_esc >= NUM_CONTROLS` guard for the equivalent pattern. A non-motor `actuator_function` underflows `i_esc` to a
large unsigned value, producing an out-of-range shift (undefined behaviour). Confirmed via static source analysis;
PRB06 is an inert placeholder in this session's non-sanitizer build -- not runtime-verified this session.

---

### F-08 WRONG-case log message is 0-based, inconsistent with other cases   Status: confirmed
Location: FailureInjector.cpp:95 (`PX4_INFO("CMD_INJECT_FAILURE, motor %d esc telemetry wrong", i);`) -- contrast the
STUCK case (L91, `PX4_INFO("... motor %d no esc telemetry", i + 1);`)
Related tests: SQE-FI-06 (`FI06_InjectWrongMotor1_ScalesTelemetry`) -- covers the WRONG case's bitmask/scaling
behaviour, not the log text itself
Evidence: direct source read this session confirms the discrepancy exactly as REF_07 states; FI06's own assertions
(`evidence/tests/xml/functional-SqeFailureInjector.xml`, PASS) do not capture stdout, so FI06 demonstrates the
correct bitmask/value-scaling logic works, not the log-message bug -- the log-message bug itself is confirmed by
reading the source, not by a dedicated capturing test (none was written, since it is cosmetic and R4 bars
manufacturing a new assertion just to pad the finding count).
Reproduction: trigger `FAILURE_TYPE_WRONG` for logical motor instance 1 (ESC index 0) -> log reads "motor 0 esc
telemetry wrong" instead of "motor 1 ...", while the STUCK case for the same instance correctly logs "motor 1 no
esc telemetry".
Expected: consistent 1-based motor numbering in all log messages (matches every other case in the same switch).
Actual: the WRONG case alone uses the raw 0-based loop index `i`.
Classification + rationale: cosmetic (log-message-only off-by-one) -- confirmed exact line and discrepancy this
session; the underlying bitmask logic at L96 (`_esc_telemetry_wrong_mask |= 1 << i`) correctly uses 0-based `i`
(bitmask indices are legitimately 0-based), so this is strictly a human-readable log text inconsistency, not a
logic defect.
Severity / reachability: log output only; no functional/control impact.
Report text: `FailureInjector.cpp:95`'s log message for `FAILURE_TYPE_WRONG` prints the 0-based loop index `i`
("motor %d"), while every other case in the same switch (e.g. STUCK at L91) prints `i + 1` for 1-based motor
numbering. Confirmed by direct source read this session. Cosmetic only -- the actual bitmask logic (L96) is correct.

---

### F-09 Non-finite (NaN) stream stays "healthy" (confidence 1.0, NO_ERROR)   Status: confirmed -- HUMAN-DECISION (defect vs. specification ambiguity)
Location: DataValidator.cpp:68 (`if (PX4_ISFINITE(val[i]))` -- guards the per-axis update body; `_time_last` is set
unconditionally at L97, outside that per-axis guard), L100-142 (`confidence()`)
Related tests: SQE-DV-07 (`DV07_AllNonFiniteStream_LeavesValueAtZeroButReportsFullConfidence`), SQE-PRB-01 (disabled probe)
Evidence: `evidence/tests/xml/unit-SqeDataValidator.xml` (DV07 PASS this session, `ctest -R Sqe`),
`evidence/tests/probes/unit-SqeDataValidator.xml` (PRB01 FAILS as designed, re-run this session)
Reproduction: `put(t, {NaN,NaN,NaN}, 0, 1)` x10 -> `used() == true`, `value() == {0,0,0}` (never written),
`confidence(t) == 1.0f`, `state() == 0` (NO_ERROR). Confirmed via DV07's direct assertions, all PASS.
Expected (per the file-header purpose statement "A data validation class to identify anomalies in data streams"):
a stream that has only ever carried non-finite values would be flagged as anomalous. Actual: `put()` updates
`_time_last` unconditionally (L97, outside the per-axis finiteness guard) even when every axis was non-finite, so
`confidence()`'s only gate (`_time_last == 0` -> NO_DATA) is satisfied by the mere fact that `put()` was called at
all, regardless of whether any finite data was ever stored.
Reachability check performed this session: read `src/modules/sensors/vehicle_imu/VehicleIMU.cpp` and
`voted_sensors_update.cpp` for upstream NaN filtering before data reaches `DataValidator::put()`. Neither file
contains an explicit `PX4_ISFINITE`/NaN-rejection check on the raw IMU sample before it is passed into the
validator chain (searched for `isfinite`/`ISFINITE`/`isnan` in both files this session; no matches). This means a
NaN sample reaching `put()` is not provably filtered upstream in this code path, though this session did not trace
every possible sensor driver (hardware I2C/SPI drivers are out of this assignment's scope and not built in SITL).
Classification + rationale: candidate defect vs. documented purpose -- HUMAN-DECISION required. The code's
behaviour is internally consistent and arguably intentional (NaN inputs are simply ignored per-axis, not treated as
"errors" via the explicit `error_count` channel); whether "identify anomalies" in the header is a binding contract
that NaN-only streams must violate is a specification question this session cannot resolve unilaterally (R4).
Reported honestly as both a real, reproduced behaviour (DV07) and a live disagreement with a stricter oracle (PRB01
fails under that oracle).
Severity / reachability: if a NaN reaches `put()`, that sensor would be reported "healthy"/confidence 1.0 and
remain selectable by `DataValidatorGroup::get_best()` -- this session's reading of `VehicleIMU.cpp`/
`voted_sensors_update.cpp` did not find an upstream filter that definitively prevents this for the IMU path, but a
full trace of every sensor driver was not performed (out of scope).
Report text: A stream of only non-finite (NaN/Inf) samples is reported as fully healthy (`confidence()==1.0`,
`state()==NO_ERROR`) because `_time_last` is updated unconditionally in `put()` regardless of whether any axis was
finite. Confirmed via DV07 (characterization) and PRB01 (fails under the stricter "should reduce confidence"
oracle). This session's source read found no explicit NaN-rejection in `VehicleIMU.cpp`/`voted_sensors_update.cpp`
upstream of `put()`, but did not exhaustively trace every sensor driver. HUMAN-DECISION needed: is "identify
anomalies in data streams" (file header) a binding contract this violates, or is per-axis NaN-ignoring by design?

---

### F-10 Error density exactly at window boundary -> confidence 0, no flag   Status: confirmed -- HUMAN-DECISION (defect vs. specification ambiguity)
Location: DataValidator.cpp:125 (`else if (_error_density > ERROR_DENSITY_WINDOW)` -- strict `>`, not `>=`)
Related tests: SQE-DV-15 (`DV15_DensityExactlyAtWindow_ZeroConfidenceButNoFlag`), SQE-PRB-02 (disabled probe)
Evidence: `evidence/tests/xml/unit-SqeDataValidator.xml` (DV15 PASS this session), `evidence/tests/probes/unit-SqeDataValidator.xml`
(PRB02 FAILS as designed, re-run this session)
Reproduction: `put(T0, v, error_count=100, 0)` -> `confidence(T0) == 0.0f` (via `ret = 1.0f - 100/100.0f == 0`), but
`state() == 0` (no `ERROR_FLAG_HIGH_ERRDENSITY`) because `100 > 100` is False. Confirmed via DV15's assertions, PASS.
Expected (per a stricter reading implying `>=`): any confidence-zero condition should be explained by a non-zero
state flag. Actual: `state()==0` is indistinguishable from "perfectly healthy" by flag inspection alone, even though
`confidence()==0` (the worst possible score).
Classification + rationale: boundary inconsistency -- HUMAN-DECISION required (defect: `>` should be `>=`, vs.
specification ambiguity: the density cap and the flag threshold are intentionally allowed to diverge by one count).
Consumer impact check performed this session: read `voted_sensors_update.cpp:419` --
`status.accel_healthy[i] = (_accel.voter.get_sensor_state(i) == DataValidator::ERROR_FLAG_NO_ERROR);` confirms the
exact line and exact pattern REF_07 cites: `get_sensor_state()` returning `0` (`ERROR_FLAG_NO_ERROR`) at this
boundary makes `status.accel_healthy[i]` report `true` for one cycle even though the validator's own `confidence()`
is simultaneously at its minimum (0).
Severity / reachability: a one-cycle "healthy" telemetry report (`sensors_status_imu.accel_healthy[i] == true`)
coincides with `confidence()==0` at exactly this boundary -- a narrow, single-density-count window, not a sustained
failure mode.
Report text: At `_error_density == ERROR_DENSITY_WINDOW` (100) exactly, `confidence()` returns 0 (the minimum
possible value) but `state()` reports no flag (`100 > 100` is False, so `ERROR_FLAG_HIGH_ERRDENSITY` is not set).
Confirmed via DV15 (characterization) and PRB02 (fails under the stricter "flag should be set" oracle). Consumer
impact confirmed: `voted_sensors_update.cpp:419` uses `state() == NO_ERROR` as its "healthy" test, so this boundary
produces one cycle of `accel_healthy[i] == true` reporting alongside zero confidence. HUMAN-DECISION needed: is
`>` vs `>=` at L125 a defect, or an intentional one-count gap between the density cap and the flag threshold?

---

### F-11 Equal-priority confidence-driven switch counted as a failover   Status: confirmed -- HUMAN-DECISION (classification semantics)
Location: DataValidatorGroup.cpp:207-227 (the "check whether the switch was a failsafe or preferring a higher
priority sensor" block)
Related tests: SQE-DVG-MC-04 (`MC04_HigherConfidenceEqualPriority_SwitchCountedAsFailover`), SQE-DVG-MC-07
(`MC07_BelowThresholdConfidenceReplacedByAboveThreshold_Switches`)
Evidence: `evidence/tests/xml/unit-SqeDataValidatorGroup.xml` (MC04, MC07 both PASS this session, `ctest -R Sqe`)
Reproduction: MC04 -- sensor 0 best (conf .95, prio 50), sensor 1 appears with higher confidence (.97) and equal
priority (50) -> `get_best()` switches to sensor 1, and `failover_count()` becomes 1. Confirmed via MC04's
assertions (`EXPECT_EQ(idx,1); EXPECT_EQ(g.failover_count(), 1u);`), PASS.
Expected: the source's own comment at L206 ("check whether the switch was a failsafe or preferring a higher
priority sensor") implies the intent is to distinguish "real failsafe" from "preferring a higher priority sensor" --
it does not explicitly describe the equal-priority, confidence-only case.
Actual: the guard at L207-208 (`pre_check_prio != -1 && pre_check_prio < max_priority && ...`) only suppresses the
failsafe count when the new sensor's priority is strictly higher (`pre_check_prio < max_priority`); an
equal-priority improvement (`pre_check_prio < max_priority` is False when equal) falls through to `true_failsafe =
true`, incrementing `_toggle_count` -- i.e. `failover_count()`.
Classification + rationale: classification semantics (ambiguity) -- HUMAN-DECISION required. Two readings
coexist, per REF_07's instruction to present both rather than call it a defect without a specification: (1) the
code comment's literal intent (distinguish "priority preference" from "real failsafe") is correctly implemented for
the priority case specifically, and an equal-priority confidence improvement was perhaps never considered a
"preference" case by the original author, so counting it as a failover is arguably intentional/consistent; (2) from
a monitoring/ops perspective, an equal-priority, confidence-only switch between two healthy sensors is arguably not
a "failsafe" event and inflating `failover_count()` could mislead a log/telemetry consumer into over-counting real
failures.
Severity / reachability: `failover_count()` increments (observable, used in telemetry/logging), but
`failover_index()` returns -1 for this case (no specific sensor is blamed) since `_prev_best` bookkeeping does not
flag a specific failed sensor -- confirmed via MC04/MC07's structure (no explicit `failover_index()` assertion in
either test, consistent with REF_07's "no message because `failover_index()` = -1" note, cross-checked against the
function body at DataValidatorGroup.cpp:276-292 which only matches `next->state() != ERROR_FLAG_NO_ERROR`, true for
neither sensor in this equal-priority-improvement scenario).
Report text: A confidence-only switch between two equal-priority, both-healthy sensors (MC04, MC07) increments
`failover_count()`, even though the source comment's stated intent is to distinguish "a real failsafe" from
"preferring a higher priority sensor" -- the equal-priority case is not an instance of either description taken
literally. Confirmed via direct execution (MC04/MC07 PASS with these exact assertions). HUMAN-DECISION needed:
is this the intended behaviour (any non-priority-driven switch = failover) or should equal-priority confidence
improvements be excluded from the failover count?

---

### F-12 Timestamp 0 doubles as the "no data" sentinel                Status: confirmed
Location: DataValidator.cpp:69 (`if (_time_last == 0) { ...init...}` inside `put()`), L106 (`if (_time_last == 0)`
inside `confidence()`)
Related tests: SQE-DV-20 (`DV20_TimestampZero_NeverMarksUsedAndReInitsOnNextPut`), SQE-DVG-MC-25
(`MC25_PutWithTimestampZeroResetsUsed_NotReportedAsFailover`)
Evidence: `evidence/tests/xml/unit-SqeDataValidator.xml` (DV20 PASS), `evidence/tests/xml/unit-SqeDataValidatorGroup.xml`
(MC25 PASS) -- both confirmed this session via `ctest -R Sqe`
Reproduction: `put(0, v1)` (timestamp literally 0) -> `used() == false` even though `value()` was written with `v1`;
`confidence(0) == 0`, `state() == NO_DATA`. A subsequent `put(1000, v2)` re-enters the "first sample" init branch
(because `_time_last` was still 0), so `rms()` resets to `{0,0,0}` instead of computing a real delta from `v1`->`v2`.
Correction to REF_07: REF_07 cites "MC17" as evidence; no `MC17` test exists in the actual suite (confirmed via
`grep -n "^TEST_F" SqeDataValidatorGroupTest.cpp` this session). The DVG-level test that actually exercises the
timestamp-0 mechanism is MC25, not MC17 -- noted here as a correction; the finding itself is unaffected and MC25's
assertions directly confirm it.
Expected/Actual: timestamp 0 is used as a sentinel meaning "never received data", which is reasonable given PX4's
`hrt_absolute_time()` starts counting from boot and is never literally 0 after boot -- but it does mean a
caller that (mis)uses timestamp 0 as a real value gets silently treated as "no data ever received".
Classification + rationale: boundary observation, confirmed by execution -- low severity, matches REF_07's own
"timestamps are never 0 after boot" framing (the sentinel is practically safe in the real system).
Severity / reachability: none in practice (as REF_07 notes, `hrt_absolute_time()` is never 0 post-boot); only
reachable via direct unit-level misuse (exactly what DV20/MC25 exercise).
Report text: `DataValidator` uses timestamp `0` as an implicit "no data ever received" sentinel (`_time_last == 0`
gates both `put()`'s init branch and `confidence()`'s `NO_DATA` flag). Confirmed via DV20 (unit level) and MC25
(group level), both PASS. Not reachable in the real system since `hrt_absolute_time()` is never 0 after boot;
flagged as a boundary characteristic, not a live risk.

---

### F-13 Constructor/`add_new_validator()` not robust to allocation failure   Status: confirmed -- latent, not runtime-verified this session
Location: DataValidatorGroup.cpp:55 (`next = new DataValidator();`, constructor), L86-92 (`add_new_validator()`)
Related tests: none in the submitted suite -- the optional `AF02` allocation-fault-injection test was judged in
P08/P10 not worth a separate sanitizer/allocator build and was never implemented (see `work/GAPS.md` G-01, same
session's finding)
Evidence: no executable evidence this session -- REF_07's own header states this was pre-verified by compiling the
v1.17.0 sources standalone with stubs (not in the PX4 GTest suite); this session did not reproduce that standalone
build, and no `AF*`-series test exists in the actual suite to provide PX4-GTest-level evidence
Reproduction: not performed this session (would require a custom allocator override / `-fno-exceptions` build --
the same missing capability documented in `work/GAPS.md` G-01 for the related, but distinct, `-fcheck-new` dead-code
observation)
Expected/Actual: not executed -- this session can only confirm the code shape (L55/L86 call `new DataValidator()`
with no null check beyond the already-dead-code L88 guard, see G-01) by reading the source, not that allocation
failure actually causes the claimed SIGSEGV in a live run.
Classification + rationale: latent, unverified-at-runtime-this-session -- downgraded from REF_07's provisional
"pre-verified: SIGSEGV" framing because that pre-verification was done outside the PX4 GTest suite (per REF_07's own
header disclaimer) and was not independently reproduced this session. Per R3/R12, this session reports only what it
itself confirmed: the code shape (no null-safe allocation handling) is real (read directly), but the runtime
consequence is not this session's own evidence.
Severity / reachability: memory exhaustion at startup -- an environment-dependent scenario (NuttX allocator
returning null vs. this platform's throwing `operator new`), connected to `work/GAPS.md` G-01's `-fcheck-new`
dead-code analysis (same underlying allocation-failure-handling gap, viewed from two angles: G-01 is about why the
existing null check is unreachable here; F-13 is about whether the code would survive if it were reachable).
Report text: Neither `DataValidatorGroup`'s constructor nor `add_new_validator()` null-checks the result of `new
DataValidator()` in a way that is reachable on this platform (see G-01: the existing check at L88 is dead code
under this build's throwing-`operator new` ABI). Whether allocation failure would actually cause a crash was not
verified by a PX4 GTest in this session (no `AF*` test exists in the submitted suite); reported as a latent,
code-shape-only observation, not a reproduced runtime defect.

---

### F-14 `_first_failover_time`/`reset_state()` effects not observable via public API   Status: confirmed (testability observation)
Location: DataValidatorGroup.cpp:214 (`best->reset_state();`), L230-231 (`_first_failover_time`)
Related tests: SQE-DVG-MC-21 (`MC21_FirstCallKFalseControlVector_NoFailoverCounted`) + per-test structural coverage
Evidence: `evidence/coverage/pertest/MC21_O3_EVIDENCE.md` (captured in P10, cross-referenced and read in full this
session), `evidence/coverage/pertest/MC21_evidence.info` / `MC01_evidence.info` (raw per-test line-hit data)
Reproduction: `MC21` (K=False row) reaches line 210 (`true_failsafe = false;`) 0 times; `MC01` (K=True,
independence-pair row) reaches the same line 1 time -- confirmed via `DA:210,0` vs `DA:210,1` in the two
`.info` files, re-read this session.
Expected/Actual: this is a testability characteristic, not a behavioural expectation -- no getter (`failover_count()`,
`get_sensor_state()`, etc.) exposes whether `true_failsafe` was true or false in the K=False row, because K=False
structurally forces `_curr_best < 0` at L219, making `true_failsafe`'s value irrelevant to every externally-visible
output in that call. The only way to show the test reached its intended code path is structural (line) coverage.
Classification + rationale: testability observation, confirmed -- matches REF_07's "weak oracle only" framing
exactly; no severity beyond affecting how confidently a black-box test can claim it exercised this specific path.
Severity / reachability: none (not a behavioural defect) -- purely an observation about test design limits for this
function.
Report text: `DataValidatorGroup::get_best()`'s `true_failsafe` variable (and the `best->reset_state()` call it
gates at L213-214) has no getter-visible effect when the enclosing decision is reached via its K=False path (the
"first call" control-vector scenario), because that path independently forces `_curr_best < 0`. Confirmed via
per-test structural coverage (`MC21_O3_EVIDENCE.md`): line 210 is hit 0 times in MC21 vs. 1 time in the paired
MC01 -- proof the test reached the intended branch structurally, since no API oracle can distinguish the two cases.

---

### F-15 Motor-timed-out mask survives disarm                        Status: confirmed
Location: FailureDetector.cpp:345-353 (the "Disarmed" else-branch: resets `_motor_failure_undercurrent_start_time[]`
and `_motor_failure_esc_under_current_mask`, never `_motor_failure_esc_timed_out_mask`)
Related tests: SQE-FD-33 (`FD33_DisarmAfterTimeout_MotorFlagClearsButTimedOutMaskSurvivesCharacterization`)
Evidence: `evidence/tests/xml/functional-SqeFailureDetector.xml` (FD33 PASS this session, `ctest -R Sqe`)
Reproduction: ESC0 timed out via telemetry timeout -> `getStatusFlags().motor == true`,
`getMotorFailures() & 0x01 == 0x01`. Then `vehicle_status.arming_state = DISARMED`, republish -> `getStatusFlags().motor
== false` (correctly reset), but `getMotorFailures() & 0x01` still == 0x01 (confirmed via FD33's exact
assertions, PASS).
Expected (reasonable operator expectation): a full disarm would clear all latched motor-failure bookkeeping before
the next arm. Actual: read `FailureDetector.cpp:345-353` this session -- the disarmed-reset block only clears
`_motor_failure_esc_under_current_mask` and the undercurrent start-time array; `_motor_failure_esc_timed_out_mask`
is never touched in that block, so it survives across a disarm and would still be set on the next `getMotorFailures()`
call after a subsequent re-arm, even though `flags.motor` itself was correctly reset to false.
Classification + rationale: observation/characterization (possible stale report after re-arm), confirmed by
execution -- not labeled HUMAN-DECISION since REF_07 itself frames it as a straightforward observation, and this
session's reading confirms the asymmetry is unambiguous (one mask reset, the other mask untouched, in the same
block, same function).
Severity / reachability: `getMotorFailures()` (the public accessor for this bitmask) would report motor 1 as
previously timed-out even after a clean disarm/re-arm cycle, which could be misleading to any caller treating a
non-zero `getMotorFailures()` as "currently failing" rather than "has ever failed since last explicit mask clear".
Report text: `FailureDetector`'s disarm-handling block (`FailureDetector.cpp:345-353`) resets
`_motor_failure_esc_under_current_mask` but never `_motor_failure_esc_timed_out_mask`, so a motor that timed out
before disarm still reports as failed in `getMotorFailures()` after disarm, even though `getStatusFlags().motor`
(the aggregate flag) is correctly cleared. Confirmed via FD33's direct execution.

---

## Sanitizer evidence for F-07a/F-07b -- not attempted this session
Per the task's own risk/time guidance (a full `PX4_ASAN=1`/`PX4_UBSAN=1` rebuild replacing the working Coverage
build, 15-20+ min each, with real risk of disrupting a build other phases may still need), this session did not
attempt the optional sanitizer rebuild in SPEC_10 step 5 / P11 plan step 3. F-07a/F-07b are therefore reported as
"confirmed via static source analysis only, not yet runtime-verified under a sanitizer build" (per R12, not
overclaimed as sanitizer-confirmed). PRB05/PRB06 were re-run this session in the normal Coverage build and
confirmed to execute as inert `SUCCEED()` placeholders (`evidence/tests/probes/functional-SqeFailureInjector_PRB0506.{xml,log}`),
consistent with their documented design (UB only manifests, and is only detected, under ASan/UBSan). The Coverage
build was not touched and remains verified Coverage (`grep CMAKE_BUILD_TYPE PX4-Autopilot/build/px4_sitl_test/CMakeCache.txt`
-> `Coverage`, checked both before and after this session's work).

## HUMAN-DECISION items (this phase)
- F-09 -- defect (NaN stream reported healthy) vs. specification ambiguity (per-axis NaN-ignoring is by design;
  `error_count` is the intended anomaly-signalling channel, not finiteness).
- F-10 -- defect (`>` should be `>=` at L125) vs. specification ambiguity (intentional one-count gap between the
  density cap and the flag threshold).
- F-11 -- classification semantics: should an equal-priority, confidence-only sensor switch count as a
  `failover_count()` event, or only priority-driven/true-failsafe switches?
- (Carried from P04, unrelated to this phase's own findings, listed in STATUS for completeness) D-004 exclusions
  list, and probe oracles PRB-01/02/03 themselves (the three probes above are the concrete artefacts of the F-09/
  F-10/F-02 human-decision items -- approving an oracle for a probe is resolving the corresponding classification).
