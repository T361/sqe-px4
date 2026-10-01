# REPORT_BLOCKS -- ready-to-use defect/finding text for the P13 report Sections 7-8
Source: `work/FINDINGS.md` (SPEC_10 format), condensed per SPEC_10 step 7's defect-format fields: title, source
location, reproduction conditions, expected, actual, test ID, evidence path, severity rationale. Only CONFIRMED
findings are included below (13 of REF_07's 15 candidates; F-07a/F-07b included as "confirmed, static-analysis-only"
since that is itself a defensible, non-overclaiming confirmed status per SPEC_10's classes). Language follows R12
(no overclaiming; "candidate defect" / "confirmed defect" / "specification ambiguity" / "characterization" used
distinctly, matching each finding's actual classification in work/FINDINGS.md).

---

## Block 1 -- F-01: Dead null-check in `get_best()` (structural observation)
**Title:** `best != nullptr` guard at `DataValidatorGroup.cpp:213` is unreachable in its False branch
**Source location:** `DataValidatorGroup.cpp:213`
**Reproduction conditions:** n/a -- proof by control-flow dominance (not an executable reproduction)
**Expected:** both branches of the guard would be independently reachable (standard branch-coverage assumption)
**Actual:** the guard's False side (`best == nullptr`) cannot occur: every path that reaches the enclosing condition
also unconditionally sets `best` non-null in the same code block (L158-169)
**Test ID:** none (structural proof) -- documented in `work/GAPS.md` G-03, `work/basis/CFG_DataValidatorGroup_get_best.md`
**Evidence path:** `work/GAPS.md` (G-03), `work/mcdc/MCDC_ANALYSIS.md`
**Severity rationale:** none -- not a behavioural defect, a measurement/coverage artefact of redundant defensive code

---

## Block 2 -- F-02: `get_best()` returns a stale non-null pointer on total sensor failure
**Title:** `get_best()` returns a non-null pointer to stale data when `*index == -1` signals total failure
**Source location:** `DataValidatorGroup.cpp:234-245`
**Reproduction conditions:** construct `DataValidatorGroup(2)`; make sensor 0 best; let both sensors time out; call
`get_best()` again
**Expected:** per the header's "pointer to the array of best values" phrasing, a null pointer once nothing is
selected
**Actual:** a non-null pointer to the previously-best sensor's last-known values, with `*index == -1` as the only
failure signal
**Test ID:** SQE-DVG-11, SQE-DVG-MC-19 (confirm); SQE-PRB-03 (disabled probe, fails under the stricter nullptr
oracle)
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidatorGroup.xml`, `evidence/tests/probes/unit-SqeDataValidatorGroup.xml`
**Severity rationale:** latent -- both production callers (`voted_sensors_update.cpp:195-196`,
`VehicleMagnetometer.cpp:536`) discard the returned pointer and use only `*index`, so no current flight-code path is
affected; risk is to future callers that trust the header's documented contract.

---

## Block 3 -- F-03: Null-pointer crash on `DataValidatorGroup(0)` + `add_new_validator()`
**Title:** `add_new_validator()` dereferences a null `_last` pointer when the group was constructed with 0 siblings
**Source location:** `DataValidatorGroup.cpp:67` (construction leaves `_last == nullptr`), `:92` (unguarded deref)
**Reproduction conditions:** `DataValidatorGroup g(0); g.add_new_validator();`
**Expected:** defined behaviour or a documented precondition for the zero-sibling case
**Actual:** unconditional SIGSEGV
**Test ID:** SQE-DVG-10 (death test)
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidatorGroup.xml`
**Severity rationale:** unreachable from production -- all 3 real construction sites (`voted_sensors_update.h:114`,
`VehicleAirData.hpp:121`, `VehicleMagnetometer.hpp:157`) use `{1}`. Latent robustness gap only, confirmed by a
grep of every production construction site this session.

---

## Block 4 -- F-04: Shared stale-value counter across padded axes
**Title:** scalar (1-D) sensors reach the stale-data threshold faster than genuinely 3-axis sensors
**Source location:** `DataValidator.cpp:82-87`
**Reproduction conditions:** 34x `put(t, 7.f, 0, 1)` via the scalar overload (pads axes 1/2 with 0.0f)
**Expected:** documented design trade-off (single shared counter across all 3 axes)
**Actual:** `state()` reports `ERROR_FLAG_STALE_DATA` after the 35th call; the two zero-padded axes contribute
"equal" votes every cycle alongside the real axis
**Test ID:** SQE-DV-05
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidator.xml`
**Severity rationale:** specification ambiguity (documented trade-off), real consumer impact confirmed at
`sensors.cpp:285` (airspeed validator uses exactly this padded single-channel pattern) -- reaches "stale" roughly
3x faster than a true 3-axis constant signal would.

---

## Block 5 -- F-05: `print()` is not read-only
**Title:** `DataValidator::print()` mutates `_error_mask` via its internal `confidence()` call
**Source location:** `DataValidator.cpp:152-153`
**Reproduction conditions:** `put(1000, val, 0, 0)` then `print()` (no explicit `confidence()` call)
**Expected:** a diagnostic/print function should not mutate observable state
**Actual:** `state()` reports `ERROR_FLAG_TIMEOUT` after only `put()` + `print()`, because `print()` calls
`confidence(hrt_absolute_time())` once per axis
**Test ID:** SQE-DV-19
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidator.xml`
**Severity rationale:** low -- `sensors status` (the operator diagnostic command) can set TIMEOUT flags purely by
being invoked; could confuse a subsequent `state()` read elsewhere.

---

## Block 6 -- F-06: 1% confidence tie-break is float-rounding/optimisation-sensitive
**Title:** `DataValidatorGroup`'s 1% equal-confidence switch threshold is sensitive to compiler optimisation level
**Source location:** `DataValidatorGroup.cpp:189`, `DataValidator.cpp:134`
**Reproduction conditions:** n/a -- characterised via direct float32 arithmetic, not a failing test (the submitted
MC/DC vectors deliberately avoid this boundary)
**Expected:** a 1% difference threshold implies a clean decimal boundary
**Actual:** `1.0f - d/100.0f` in IEEE-754 float32 is not exact at integer `d` (e.g. `d=10` -> `0.89999998f`); the
resulting adjacent-step classification can differ between `-O0` and `-O2 -freciprocal-math` builds (per REF_07's
pre-session measurement, reproduced here only via hand arithmetic, not a second build)
**Test ID:** none directly exercising the boundary (by design, for stability) -- `work/mcdc/MCDC_ANALYSIS.md` section 4
**Evidence path:** `work/mcdc/MCDC_ANALYSIS.md`
**Severity rationale:** specification ambiguity / evidence-transfer limitation -- a real algorithmic characteristic,
not exercised by the submitted suite's stable test vectors.

---

## Block 7 -- F-07a: Unclamped ESC-count loop in `FailureInjector::manipulateEscStatus`
**Title:** out-of-bounds array write possible when `esc_status.esc_count > CONNECTED_ESC_MAX`
**Source location:** `FailureInjector.cpp:117`
**Reproduction conditions:** `SYS_FAILURE_EN=1`; inject a failure; publish `esc_status` with `esc_count=9` (fixed
array is `esc[8]`)
**Expected:** a clamp to `CONNECTED_ESC_MAX`, matching the sibling loop at `FailureInjector.cpp:67`
**Actual:** no clamp; out-of-bounds write is undefined behaviour, undetected without a sanitizer
**Test ID:** SQE-PRB-06 is a different probe -- this is SQE-PRB-05 (disabled; runs as an inert `SUCCEED()` in the
normal build, confirmed this session)
**Evidence path:** `evidence/tests/probes/functional-SqeFailureInjector_PRB0506.xml`
**Severity rationale:** robustness defect, confirmed by static source analysis only -- **not runtime-verified this
session** (no ASan rebuild attempted, time/risk judgment call, see work/FINDINGS.md). Reachable only with failure
injection active and an out-of-contract `esc_count`, not through normal flight paths.

---

## Block 8 -- F-07b: Unguarded shift exponent in `FailureInjector::manipulateEscStatus`
**Title:** unguarded `1 << i_esc` with a potentially huge unsigned `i_esc` for non-motor ESC functions
**Source location:** `FailureInjector.cpp:118-126`, contrast guarded equivalent at `FailureDetector.cpp:274`
**Reproduction conditions:** `SYS_FAILURE_EN=1`; inject a failure; publish `esc_status` with
`esc[0].actuator_function = 0` (not motor-derived)
**Expected:** a range guard consistent with `FailureDetector.cpp:274`'s `i_esc >= NUM_CONTROLS` check
**Actual:** no guard; `i_esc` underflows to a large unsigned value, producing an out-of-range shift (UB)
**Test ID:** SQE-PRB-06 (disabled; inert `SUCCEED()` in the normal build, confirmed this session)
**Evidence path:** `evidence/tests/probes/functional-SqeFailureInjector_PRB0506.xml`
**Severity rationale:** undefined behaviour, confirmed by static source analysis only -- not runtime-verified this
session. Same injection-only reachability constraint as F-07a.

---

## Block 9 -- F-08: 0-based motor index in WRONG-case log message
**Title:** `FAILURE_TYPE_WRONG` log message is the only case using a 0-based motor index
**Source location:** `FailureInjector.cpp:95` (contrast L91's `i + 1`)
**Reproduction conditions:** trigger `FAILURE_TYPE_WRONG` for logical motor instance 1 (ESC index 0)
**Expected:** 1-based motor numbering consistent with every other case in the same switch
**Actual:** the log text reads "motor 0" instead of "motor 1"; the underlying bitmask logic (L96) is correctly
0-based and unaffected
**Test ID:** none captures the log text directly (SQE-FI-06 confirms the correct bitmask/scaling behaviour only) --
confirmed by direct source read, not by a dedicated stdout-capturing test (R4: no assertion manufactured just to
pad the finding)
**Evidence path:** `PX4-Autopilot/src/modules/commander/failure_detector/FailureInjector.cpp:91,95,96` (source read)
**Severity rationale:** cosmetic, log-message only, no functional/control impact.

---

## Block 10 -- F-09: NaN-only stream reports as fully healthy (HUMAN-DECISION)
**Title:** a stream of only non-finite samples is reported as `confidence()==1.0`, `state()==NO_ERROR`
**Source location:** `DataValidator.cpp:68` (per-axis finiteness guard), `:97` (`_time_last` updated unconditionally)
**Reproduction conditions:** `put(t, {NaN,NaN,NaN}, 0, 1)` x10
**Expected (per file header "identify anomalies in data streams"):** a NaN-only stream flagged as anomalous
**Actual:** `used()==true`, `value()=={0,0,0}` (never written), `confidence()==1.0`, `state()==0`
**Test ID:** SQE-DV-07 (confirms the characterization); SQE-PRB-01 (disabled, fails under the stricter oracle)
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidator.xml`, `evidence/tests/probes/unit-SqeDataValidator.xml`
**Severity rationale:** candidate defect vs. documented purpose -- **HUMAN-DECISION required**. This session's
reading of `VehicleIMU.cpp`/`voted_sensors_update.cpp` found no explicit upstream NaN rejection before data reaches
`put()`, but did not exhaustively trace every sensor driver (out of scope). If reachable in a live system, a NaN
sensor would remain selectable by `get_best()`.

---

## Block 11 -- F-10: Error density at exact window boundary gives zero confidence with no flag (HUMAN-DECISION)
**Title:** `_error_density == ERROR_DENSITY_WINDOW` (100) yields `confidence()==0` but no `HIGH_ERRDENSITY` flag
**Source location:** `DataValidator.cpp:125` (`>` not `>=`)
**Reproduction conditions:** `put(T0, v, error_count=100, 0)`
**Expected:** a confidence-zero result explained by a non-zero state flag
**Actual:** `confidence()==0`, `state()==0` (NO_ERROR) -- the minimum confidence score is indistinguishable from
"healthy" by flag inspection
**Test ID:** SQE-DV-15 (confirms); SQE-PRB-02 (disabled, fails under the stricter oracle)
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidator.xml`, `evidence/tests/probes/unit-SqeDataValidator.xml`
**Severity rationale:** boundary inconsistency -- **HUMAN-DECISION required**. Confirmed consumer impact at
`voted_sensors_update.cpp:419`: `status.accel_healthy[i]` reports `true` for one cycle at this exact boundary
despite zero confidence.

---

## Block 12 -- F-11: Equal-priority confidence switch counted as a failover (HUMAN-DECISION)
**Title:** a confidence-only switch between two equal-priority, healthy sensors increments `failover_count()`
**Source location:** `DataValidatorGroup.cpp:207-227`
**Reproduction conditions:** sensor 0 best (conf .95, prio 50); sensor 1 appears with higher confidence (.97),
equal priority (50)
**Expected (per the code's own comment, "check whether the switch was a failsafe or preferring a higher priority
sensor"):** ambiguous for the equal-priority case -- not explicitly described either way
**Actual:** the guard only suppresses the failsafe count for a strictly-higher-priority candidate; an
equal-priority improvement falls through and increments `failover_count()`
**Test ID:** SQE-DVG-MC-04, SQE-DVG-MC-07
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidatorGroup.xml`
**Severity rationale:** classification semantics -- **HUMAN-DECISION required**; two legitimate readings of intent
coexist (see work/FINDINGS.md for both). `failover_index()` returns -1 for this case, so no sensor is blamed by
name, but the aggregate count is affected.

---

## Block 13 -- F-12: Timestamp 0 as implicit "no data" sentinel
**Title:** `DataValidator` treats timestamp `0` as meaning "no data has ever been received"
**Source location:** `DataValidator.cpp:69`, `:106`
**Reproduction conditions:** `put(0, v1)` (timestamp literally 0)
**Expected/Actual:** `used()==false` despite `value()` holding `v1`; `confidence(0)==0`, `state()==NO_DATA`; the
next `put()` re-enters the "first sample" init branch
**Test ID:** SQE-DV-20 (unit level), SQE-DVG-MC-25 (group level)
**Evidence path:** `evidence/tests/xml/unit-SqeDataValidator.xml`, `evidence/tests/xml/unit-SqeDataValidatorGroup.xml`
**Severity rationale:** boundary observation only -- not reachable in the real system since `hrt_absolute_time()` is
never 0 after boot.

---

## Block 14 -- F-15: Motor timed-out mask survives disarm
**Title:** `getMotorFailures()` still reports a timed-out motor after a clean disarm
**Source location:** `FailureDetector.cpp:345-353`
**Reproduction conditions:** ESC0 times out while armed (`getMotorFailures() & 0x01 == 0x01`); disarm; republish
`esc_status`
**Expected:** a full disarm clears all latched motor-failure bookkeeping
**Actual:** `getStatusFlags().motor` correctly resets to false, but `getMotorFailures() & 0x01` remains `0x01` --
the disarm-reset block clears only `_motor_failure_esc_under_current_mask`, never `_motor_failure_esc_timed_out_mask`
**Test ID:** SQE-FD-33
**Evidence path:** `evidence/tests/xml/functional-SqeFailureDetector.xml`
**Severity rationale:** characterization/observation -- could mislead a caller treating a non-zero
`getMotorFailures()` as "currently failing" rather than "has ever failed since last mask clear".

---

## Not included as report blocks (testability/latent-only, lower report priority but retained in work/FINDINGS.md)
- **F-13** (allocation-failure robustness) -- latent, code-shape-only; no PX4 GTest evidence this session (no AF*
  test implemented). Mention in §6 (gaps) rather than §7/8 (findings), cross-referenced to `work/GAPS.md` G-01.
- **F-14** (testability observation, O3-only effects) -- no behavioural claim; useful for a testability-discussion
  paragraph in the report rather than a defect block, since it has no severity.
