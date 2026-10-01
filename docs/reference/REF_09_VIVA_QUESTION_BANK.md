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
12. **Baseline vs final for DataValidatorGroup?** Baseline partial via `sitl-imu_filtering` (single fake IMU, nominal path, `sensors status`); failover
    logic never executed; final: <your numbers from evidence/coverage/final/per_file.md>.
13. **Which test covers FailureInjector L117 loop?** FI05/FI06 (manipulateEscStatus with active masks).

## MC/DC
14. **Why is data_validator the critical component?** It selects which redundant IMU/mag feeds the estimator and declares/report failovers
    (VotedSensorsUpdate::checkFailover lowers the failed sensor's priority and emits the failure event).
15. **Name the conditions of L187–190.** A m<0.9, B c≥0.9, C c>m, D p≥mp, E |c−m|<0.01, F p>mp, G c>0.
16. **Explain the E pair.** MC01 (candidate 0.95/75) vs MC03 (0.93/75): only E changes (0 vs 0.02 difference); outcome T→F; index 1 vs 0.
17. **Why does B appear as NE in MC01?** A is False, so `A && B` short-circuits; B's logical value (T) is recorded as NE(T).
18. **Unique-cause vs masking?** Unique-cause: all other conditions keep their logical values. Masking: others may change if masked. All our
    pairs are unique-cause on logical values; NE operands cannot influence the outcome.
19. **How do you observe D15's outcome for the K pair?** No API-visible effect (initialisation path discards `true_failsafe`); per-test coverage
    shows L210 executed in MC01 and not in MC13 (O3).
20. **Why not MCC for L187?** 2⁷ = 128 vectors, many infeasible (F⇒D, B⇒G, (A∧B)⇒C) and exponentially costly; MC/DC needs ≥ 8.
21. **Is MC17 realistic?** Code-feasible, domain-invalid (timestamp 0); used only to show P's independence; stated in the notes.
22. **What does MC/DC not tell you?** Boundary correctness (F-06/F-10), missing requirements (F-09), oracle quality.

## Findings and judgment
23. **Is F-09 a defect?** Confirmed behaviour: NaN-only stream keeps confidence 1. Defect only relative to the class's stated purpose; severity depends
    on upstream NaN filtering → reported as <your classification>.
24. **What is F-10?** density == 100 ⇒ confidence 0 but no flag (`>` vs `>=`); consumers treat NO_ERROR as healthy.
25. **Why are probes DISABLED?** They assert the expected behaviour of confirmed findings; they fail by design, are executed explicitly and reported
    as FAIL, keeping the regular suite green for CI.
26. **What would you add with more time?** SITL/HITL runs of sensor failover, NuttX allocator fault injection, GCC 14 condition coverage,
    tests of VotedSensorsUpdate::checkFailover, property-based tests for confidence arithmetic.

## "What if" drills (predict, then run)
- DV12 with `T0+40002` → still timed out (0). DV03 threshold 5 → stale after 3rd put (6 > 5).
- MC03 with candidate 0.94 (d=6) → **do not**: adjacent 1 % step, rounding-dependent (F-06).
- Remove MC13 → lose the K independence pair (and the only O3 evidence); line/branch totals may not change (other tests execute L207) — show with
  `sqe_unique_coverage.py`.
- FD02 with TTRI 0.05 and a 40 ms wait → roll flag still false.
