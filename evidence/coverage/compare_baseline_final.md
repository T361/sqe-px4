# Baseline vs final

> The first two tables are the IT-3 capture (`evidence/coverage/final/`). **The current final figures are in the
> last section** (IT-4, after the post-audit revision).

- baseline: `evidence/coverage/baseline/scope.info`
- final: `evidence/coverage/final/scope.info`

| File | Lines baseline → final | Branches baseline → final |
|---|---|---|
| DataValidator.cpp | 43/58 (74.1%) → 58/58 (100.0%) | 21/30 (70.0%) → 30/30 (100.0%) |
| DataValidatorGroup.cpp | 101/155 (65.2%) → 154/155 (99.4%) | 54/116 (46.6%) → 110/116 (94.8%) |
| FailureDetector.cpp | 0/153 (0.0%) → 153/153 (100.0%) | 0/254 (0.0%) → 189/254 (74.4%) |
| FailureInjector.cpp | 0/62 (0.0%) → 62/62 (100.0%) | 0/57 (0.0%) → 47/57 (82.5%) |

## Branches excluding compiler-generated exception edges (G-05)

Counted from the same `.info` files: every `BRDA` entry whose block ID starts with `e` (GCC exception-unwind edge)
is excluded; all others are source-level decision/condition branches.

| File | Baseline | Final | Exception edges removed (final, all 0 hits) |
|---|---|---|---|
| DataValidator.cpp | 21/30 (70.0%) | 30/30 (100.0%) | 0 |
| DataValidatorGroup.cpp | 54/116 (46.6%) | 110/116 (94.8%) | 0 |
| FailureDetector.cpp | 0/190 (0.0%) | 189/190 (99.5%) | 64 |
| FailureInjector.cpp | 0/48 (0.0%) | 47/48 (97.9%) | 9 |
| **Total** | **75/384 (19.5%)** | **376/384 (97.9%)** | 73 |

Remaining 8 source-level branches: G-01, G-02, G-03, G-04, G-06, G-07 (see `work/GAPS.md`).

Independently reproduced on a second machine (Ubuntu 24.04.5 WSL2, GCC 13.3.0, lcov 2.0-1, plain make/ctest/lcov
commands): identical per-line and per-branch hit sets for all four files — see `evidence/repro_independent/`.

## Final after the post-audit revision (IT-4) — current final figures

Source: `evidence/coverage/final_post_audit/scope.info` (full suite, 153/153 passed), split with
`tools/sqe_branch_split.py` (`final_post_audit/branch_split.md`).

| File | Lines baseline → final | Raw branches baseline → final | Source-level branches baseline → final |
|---|---|---|---|
| DataValidator.cpp | 43/58 → 58/58 (100.0%) | 21/30 → 30/30 (100.0%) | 21/30 → 30/30 (100.0%) |
| DataValidatorGroup.cpp | 101/155 → 155/155 (100.0%) | 54/116 → 114/116 (98.3%) | 54/116 → 114/116 (98.3%) |
| FailureDetector.cpp | 0/153 → 153/153 (100.0%) | 0/254 → 189/254 (74.4%) | 0/190 → 189/190 (99.5%) |
| FailureInjector.cpp | 0/62 → 62/62 (100.0%) | 0/57 → 47/57 (82.5%) | 0/48 → 47/48 (97.9%) |
| **Total** | **144/428 → 428/428 (100.0%)** | **75/457 → 380/457 (83.2%)** | **75/384 → 380/384 (99.0%)** |

Remaining source-level branches: G-02 (DataValidatorGroup.cpp:78), G-03 (:213), G-06 (FailureInjector.cpp:44),
G-07 (FailureDetector.cpp:194). G-01 and G-04 were closed by SQE-DVG-AF-01..03 and SQE-DVG-13.
