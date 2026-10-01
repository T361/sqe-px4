# REF_09 — Viva question bank (answers keyed to this scope; confirm every number with your own evidence)

## Baseline, environment, levels
1. **Which commit did you test and how do you know?** `d6f12ad1…`; `git rev-parse HEAD` after cloning `--branch v1.17.0`; `git rev-parse v1.17.0`
   prints the annotated tag object `a5eb12d…`, not the commit.
2. **What does `make tests` run?** CMake config `px4_sitl_test`, target `test_results` → ctest: unit-*/functional-* gtests, sitl-* scripts, posix_* tests.
3. **Why a unit test for DataValidatorGroup?** Inputs are explicit function arguments; no params/uORB/time services. We link `modules__sensors` only
   to resolve platform symbols used by `print()` (same idiom as upstream ManualControlSelectorTest).
4. **Why functional for FailureDetector?** Params read at construction (`DEFINE_PARAMETERS`, `SYS_FAILURE_EN`), uORB inputs (incl. multi-instance
   IMU status), `hrt_absolute_time()` hysteresis; `gtest_functional_main` initialises uORB and params.
5. **Why not SITL?** No drivers, work queues, module lifecycle or dynamics are needed to reach any decision; SITL adds nondeterminism.
6. **How do you control time without lockstep?** Real monotonic clock; documented minimum trigger times (0.02 s, 10 ms), fixed 300/100 ms
   hysteresis; waits ≥ 1.5×; "not yet" asserted within the same `update()`; old telemetry via `now − 400 ms`.

## Structural coverage
7. **What is a gcov "branch"?** An object-code edge: one True/False pair per short-circuit operand, loop entry/exit, switch case, ternary;
   plus compiler edges (e.g. `-fcheck-new` null checks). 100 % branch ⇒ each evaluated condition took both values.
8. **Why are there more gcov branches than decisions?** One pair per short-circuit operand (+ float-compare edges): L187–L189 carry 16 branch records for one
   decision (GCC 13.3, measured); gcov attributes each edge to the line where the jump is emitted.
9. **What does "not reached ≠ False" mean in get_best?** If `_curr_best == -1`, the seeding `if` at L158 is evaluated but its True body never runs;
   inner decisions of an un-entered block contribute nothing.
10. **Explain gap G-03 (L213).** `best == nullptr` is impossible when D15 is True: K True means a current best existed and `best` was set in L162–168.
11. **Explain L88 (`!validator`).** POSIX `new` throws; only a non-throwing allocator (NuttX) returns null. Environment-limited; optional GCC fault
    injection (SPEC_02 §9) covers it on Linux.
12. **Baseline vs final for DataValidatorGroup?** Baseline partial via `sitl-imu_filtering` (single fake IMU, nominal path, `sensors status`):
    65.2% line / 46.6% branch (`evidence/coverage/baseline/per_file.md`); failover logic never executed. Final (all tests, after the student
    suite): 99.4% line (154/155) / 94.8% branch (110/116) (`evidence/coverage/final/per_file.md`) — the one uncovered line is the infeasible
    `best != nullptr` False branch at L213 (gap G-03). Whole-scope final totals: DataValidator.cpp 100.0%/100.0%, FailureDetector.cpp
    100.0%/74.4%, FailureInjector.cpp 100.0%/82.5%, combined 99.8% line / 82.3% branch across all four files
    (`evidence/coverage/final/per_file.md`).
13. **Which test covers FailureInjector L117 loop?** FI05/FI06 (manipulateEscStatus with active masks).

## MC/DC
14. **Why is data_validator the critical component?** It selects which redundant IMU/mag feeds the estimator and declares/report failovers
    (VotedSensorsUpdate::checkFailover lowers the failed sensor's priority and emits the failure event).
15. **Name the conditions of L187–190.** A m<0.9, B c≥0.9, C c>m, D p≥mp, E |c−m|<0.01, F p>mp, G c>0.
16. **Explain the E pair.** SQE-DVG-MC-01 (candidate confidence 0.95, density d=5) vs SQE-DVG-MC-03 (candidate confidence 0.93, d=7): only E
    (`|confidence−max_confidence| < 0.01f`) changes — |.95−.95|=0 (True) vs |.93−.95|≈0.02 (False, clear of the 1% quantisation boundary per
    F-06); outcome T→F (`work/mcdc/mcdc_matrix.csv`).
17. **Why does B appear as NE in MC01?** A is False, so `A && B` short-circuits; B's logical value (T) is recorded as NE(T).
18. **Unique-cause vs masking?** Unique-cause: all other conditions keep their logical values. Masking: others may change if masked. All our
    pairs are unique-cause on logical values; NE operands cannot influence the outcome.
19. **How do you observe D15's outcome for the K pair?** The real independence pair is SQE-DVG-MC-01/SQE-DVG-MC-21 (not MC13 — an earlier
    derivation draft used MC13 before the final test IDs settled; corrected during P11's register review). K=False (MC21) has no API-visible
    effect — it's the very first `get_best()` call, so `_curr_best < 0` forces the initialisation-bookkeeping branch regardless of
    `true_failsafe`'s value. Evidence is O3: per-test coverage shows `DataValidatorGroup.cpp:210` (`true_failsafe = false;`) executed once in
    MC01's capture and zero times in MC21's (`evidence/coverage/pertest/MC01_O3_EVIDENCE.md`, `MC21_O3_EVIDENCE.md`).
20. **Why not MCC for L187?** 2⁷ = 128 vectors, many infeasible (F⇒D, B⇒G, (A∧B)⇒C) and exponentially costly; MC/DC needs ≥ 8 (12 used,
    `work/mcdc/MCDC_ANALYSIS.md`).
21. **Is MC25 realistic?** (Formerly drafted as "MC17" before final test IDs settled.) `SQE-DVG-MC-25`: code-feasible, domain-invalid
    (timestamp 0 — `put()` accepts any `uint64_t` with no validation, but real `hrt_absolute_time()` is never exactly 0 after boot); used to
    show DVG-D32/D34's P condition's independence; stated explicitly in the matrix notes as a deliberate domain-invalid probe, not a realistic
    flight scenario.
22. **What does MC/DC not tell you?** Boundary correctness (F-06/F-10), missing requirements (F-09), oracle quality.

## Findings and judgment
23. **Is F-09 a defect?** Confirmed behaviour: NaN-only stream keeps confidence 1.0 and `NO_ERROR` (`DV07`, `evidence/tests/xml/unit-SqeDataValidator.xml`).
    Classified as **confirmed — HUMAN-DECISION (defect vs. specification ambiguity)** in `work/FINDINGS.md`: it's a defect only relative to the
    class's stated purpose ("identify anomalies in data streams"); whether it's actually a problem in practice depends on whether NaN can reach
    `put()` at all from the real sensor pipeline — not resolved unilaterally this session (R4/R7), left for the team/examiner to adjudicate.
24. **What is F-10?** density == 100 ⇒ confidence 0 but no flag (`>` vs `>=`); consumers treat NO_ERROR as healthy.
25. **Why are probes DISABLED?** They assert the expected behaviour of confirmed findings; they fail by design, are executed explicitly and reported
    as FAIL, keeping the regular suite green for CI.
26. **What would you add with more time?** SITL/HITL runs of sensor failover, NuttX allocator fault injection, GCC 14 condition coverage,
    tests of VotedSensorsUpdate::checkFailover, property-based tests for confidence arithmetic.

## "What if" drills (predict, then run)
- DV12 with `T0+40002` → still timed out (0). DV03 threshold 5 → stale after 3rd put (6 > 5). (Note: confirm exact scenario parameters against
  the real test IDs in `PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorTest.cpp` — the implemented tests may use slightly
  different threshold values than these illustrative examples; always check the live source before asserting a predicted outcome.)
- MC03 with candidate 0.94 (d=6) → **do not**: adjacent 1 % step, rounding-dependent (F-06).
- Remove SQE-DVG-MC-21 → lose the K independence pair for DVG-D15 (and the only O3 evidence row in the whole matrix); line/branch totals may
  not change much since other tests still execute L207 — show with `tools/sqe_unique_coverage.py`'s output for `unit-SqeDataValidatorGroup`
  (`evidence/coverage/pertest/unit-SqeDataValidatorGroup/unique_coverage.md`) rather than guessing.
- Remove SQE-DVG-MC-13 → per the actual unique-coverage analysis (`work/explain/P10.md`), this is the test that uniquely drives
  `DataValidatorGroup.cpp:303` and 2 branch outcomes at L301-302 — removing it measurably drops coverage, unlike MC21 above.
- FD02 with TTRI 0.05 and a 40 ms wait → roll flag still false.
