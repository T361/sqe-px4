# Baseline vs final
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
