# SPEC_04 — MC/DC method (binding; course notation)

## 1. Terms
- **Decision:** a Boolean expression controlling a branch (here: DVG-D13, D14, D15, D32, D34).
- **Atomic condition:** a Boolean sub-expression without logical operators (e.g. `confidence > max_confidence`). The same variable may occur in
  several conditions (**coupled conditions**, e.g. A and C both read `max_confidence`).
- **Evaluation:** one execution of the decision. In `get_best()` the decision D13 is evaluated once per sibling per call; a test may contain several
  evaluations; MC/DC is about evaluations, the workbook names the test *and* the evaluation (call#/iteration).
- **Logical value vs evaluation trace:** with C++ short-circuit `&&`/`||`, an operand may be **NE** (not evaluated). We record `NE(T)`/`NE(F)` =
  not evaluated, logical value T/F computed from the inputs (lecture 7 "logical values vs actual evaluation trace").

## 2. Criterion used
For every decision in scope and every condition X in it, the test set contains two evaluations e1, e2 such that
(1) the decision outcome differs, (2) X's value differs, (3a) **unique-cause**: every other condition has the same logical value in e1 and e2, or
(3b) **masking**: every other condition that differs is masked (cannot influence the outcome in the evaluation where it differs), with a written argument.
We use (3a) wherever feasible and report the form per pair. With short-circuiting, (3a) is judged on logical values; NE operands are recorded
but, being unevaluated, cannot affect the outcome.
Additionally (assignment requirement): each atomic condition takes T and F across the analysed evaluations, and each decision takes T and F.

## 3. Procedure per decision
1. Copy the expression verbatim (file:line); name conditions in evaluation order.
2. State coupling/feasibility constraints (e.g. F ⇒ D; B ⇒ G; (A ∧ B) ⇒ C; K false ⇔ `_curr_best == -1`).
3. Choose concrete inputs; compute each condition's logical value, then the trace, then the outcome — by hand, then confirm by execution.
4. Pair evaluations; mark unique-cause/masking; list infeasible vectors with the constraint that excludes them (never invent a test for them).
5. Decide observability: **O1** return value (`*index`, return pointer, function result) · **O2** state getter (`failover_count()`,
   `failover_index()`, `failover_state()`, `get_sensor_state()`) · **O3** structural evidence only (per-test coverage shows which lines/edges ran).
   O3 is acceptable only with the per-test coverage artefact attached (evidence/coverage/pertest/…).
6. Enter rows in `work/mcdc/mcdc_matrix.csv` (SPEC_05 §3) and run `tools/sqe_mcdc_check.py`.

## 4. Loops and state
The decision inside a loop is evaluated with state that changes during the loop (`max_confidence`, `max_priority`, `best`). Design each test so
the **target evaluation** is isolated: fix the current best with a first `get_best()` call, then introduce the candidate; the seeded best compared
with itself always yields False (B = ¬A, C = F, F = F), so the candidate's evaluation determines the outcome.

## 5. Float conditions
Choose values whose comparisons are far from rounding boundaries (≥ 0.01 away from 0.9, ≥ 0.005 away from 0.01/0.1 thresholds). The 1 % equality
test `fabsf(c - m) < 0.01f` sits exactly on the 1 % quantisation of confidence (F-06) — do not build pairs on adjacent density steps.
Measured: at `-O0` the False set is d = 3, 8, 16, 21, …; at `-O2 -freciprocal-math` it is d = 3, 9, 16, 21, … → results at these points are
build-dependent, so MC/DC evidence from the Coverage build would not transfer to them.

## 6. Minimum vs used test count
n conditions need ≥ n + 1 evaluations for unique-cause MC/DC. D13 (7 conditions) uses 12 evaluations for clarity (each pair explicit); state the
minimum and why you did not minimise (traceability and viva explainability).

## 7. What MC/DC does not give you (say it in the report)
It does not check boundaries (F-06, F-10 are boundary findings), oracle quality, missing requirements (e.g. NaN streams, F-09), or behaviour of
decisions outside the component.

## 8. Optional tool cross-check (does not replace the derivation)
GCC ≥ 14 implements masking MC/DC instrumentation: compile with `-fcondition-coverage`, report with `gcov --conditions` (`-g`), per line:
"condition outcomes covered x/y". Recipe (separate build dir, only the needed target):
```bash
cd PX4-Autopilot
cmake -S . -B build/sqe_mcdc_gcc14 -G Ninja -DCONFIG=px4_sitl_test -DCMAKE_BUILD_TYPE=Coverage \
      -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_CXX_FLAGS="-fcondition-coverage"
cmake --build build/sqe_mcdc_gcc14 --target unit-SqeDataValidatorGroup
(cd build/sqe_mcdc_gcc14 && ./unit-SqeDataValidatorGroup)
cd build/sqe_mcdc_gcc14/src/modules/sensors/data_validator/CMakeFiles/data_validator.dir && gcov-14 --conditions DataValidatorGroup.cpp.gcda
```
(Adjust object paths with `find build/sqe_mcdc_gcc14 -name 'DataValidatorGroup.cpp.gcda'`.) lcov ≥ 2.3 can import it with `--mcdc-coverage`.
Clang ≥ 18 offers `-fcoverage-mcdc` + `llvm-cov show --show-mcdc` (source-based). Save outputs in `evidence/mcdc/tool/` and compare with your matrix.
