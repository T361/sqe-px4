# SQE-DVG-MC-21 — O3 observability evidence (P08 deferred item, closed in P10)

## What this proves
`work/mcdc/mcdc_matrix.csv` row for DVG-D15 / SQE-DVG-MC-21 documents K=False (the independence-pair row for
SQE-DVG-MC-01's K=True) as having **no observable effect via any getter** — `failover_count()`,
`get_sensor_state()`, etc. all read the same regardless of whether D15's `true_failsafe = false;` assignment at
`DataValidatorGroup.cpp:210` executes or not, because K=False forces `_curr_best < 0` at L219 by construction (see
the matrix row's full proof), which makes `true_failsafe`'s value irrelevant to every reachable output this call.
The row is therefore evidenced as **O3** (structural-only: per-test line coverage, not a getter/return-value
oracle) — this file is that evidence, captured via `tools/sqe_coverage.sh pertest`.

## Commands run (reproducible)
```bash
SQE_LCOV_EXTRA_IGNORE=empty tools/sqe_coverage.sh pertest unit-SqeDataValidatorGroup \
  'SqeDvgMcdcTest.MC21_FirstCallKFalseControlVector_NoFailoverCounted' MC21_evidence

SQE_LCOV_EXTRA_IGNORE=empty tools/sqe_coverage.sh pertest unit-SqeDataValidatorGroup \
  'SqeDvgMcdcTest.MC01_EqualConfidenceHigherPriority_SwitchNotCountedAsFailover' MC01_evidence
```
`SQE_LCOV_EXTRA_IGNORE=empty` is required for single-test, single-source-file `pertest` captures of
`unit-SqeDataValidatorGroup`: the default `OBJDIRS` list includes both
`sensors/data_validator` and `commander/failure_detector`; a DataValidatorGroup-only test produces zero `.gcda` in
the `failure_detector` directory, and lcov 2.0's `geninfo` treats a `--directory` with no `.gcda` files as a hard
`ERROR: no .gcda files found` (category `empty`) rather than skipping it — which silently aborted the real
`lcov --capture` call and fell back to the "no run data" zero-record path (see `work/DECISIONS.md` D-0xx for the
full diagnosis). `--ignore-errors empty` (already a supported, documented opt-in via `SQE_LCOV_EXTRA_IGNORE` in
the existing script, see `tools/sqe_coverage.sh`'s `setup_flags()`) fixes this without modifying the script itself.

## Evidence (raw per-test line hit counts, `evidence/coverage/pertest/*_lines.txt`)
| Line | Code | MC21 (K=False) hits | MC01 (K=True) hits |
|---|---|---|---|
| 157 | `while (next != nullptr) {` (Loop 1) | 3 | 4 |
| 203 | `if (max_index != _curr_best ...)` (D14) | 1 | 2 |
| **210** | **`true_failsafe = false;`** | **0** | **1** |
| 213 | `if (best != nullptr) {` | 0 | 1 |
| 214 | `best->reset_state();` | 0 | 1 |

MC21's `get_best()` is called exactly once (the "very first call" scenario by design — see the test's Given/When),
reaching D15 (L207-208) with K=False, which short-circuits the `&&` before L210 is ever reached: **0 hits**,
confirmed by both `evidence/coverage/pertest/MC21_evidence_lines.txt` and the raw `.info` file
`evidence/coverage/pertest/MC21_evidence.info` (`DA:210,0`).

MC01 calls `get_best()` twice (call1 seeds `pre_check_prio`, call2 evaluates D15 with K=True from that seed),
reaching L210 exactly once: **1 hit**, confirmed the same way (`DA:210,1` in
`evidence/coverage/pertest/MC01_evidence.info`).

## Conclusion
The structural difference predicted by the mcdc_matrix.csv row (K=False forces the whole D15 `&&` short-circuit,
skipping L210-L214 entirely) is directly observed in per-test line coverage, independent of and in addition to the
MC/DC matrix's own logical proof that this has no getter-visible effect. This closes the P08-deferred evidence
item for SQE-DVG-MC-21 without altering `work/mcdc/MCDC_ANALYSIS.md` or `work/mcdc/mcdc_matrix.csv` (both are
read-only per the P10 task constraints) — `tools/sqe_mcdc_check.py` was not re-run since no matrix row changed,
only supporting coverage evidence was added.
