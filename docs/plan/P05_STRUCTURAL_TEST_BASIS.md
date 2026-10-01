# P05 — Structural test basis (decisions, conditions, reachability, setup)
**Goal:** every executable decision in the scope is known, with its outcomes, atomic conditions, reachability constraints, the setup
needed to control it, and the planned tests. **Owner:** ANL. **Gate:** G05. **Rubric:** Part 1 (decision points) + Part 2 (obligations).

## Inputs
REF_03 (DataValidator), REF_04 (DataValidatorGroup), REF_05 (FailureDetector + FailureInjector) — pre-derived at v1.17.0 line numbers.

## Steps
1. **Re-verify each inventory against the source.** For every row run e.g. `sed -n '187,190p' PX4-Autopilot/src/modules/sensors/data_validator/DataValidatorGroup.cpp`
   and tick `verified`. A line-number mismatch means wrong baseline or a local edit → STOP (R1/R2).
   Copy the verified tables to `work/basis/INVENTORY_<File>.md`.
2. **Obligation counts** (write in each inventory header): executable lines (from baseline `LF`), decisions, decision outcomes, atomic
   conditions, and the **gcov branch count** per file from the baseline HTML/`BRF`. Explain the difference: gcov emits one branch pair per
   short-circuit operand (`a && b` → 4 branch outcomes), loop tests, ternaries, `switch` cases (+ implicit default), and compiler-generated
   paths (the `-fcheck-new` null check after `new` appears as an *unexecuted block* marker `*`, exception edges on calls are filtered by the pipeline
   when the lcov version supports it). Measured standalone (GCC 13.3): DataValidator.cpp 58 lines / 30 branches; DataValidatorGroup.cpp 155 / 116.
3. **Setup map** (`work/basis/SETUP_MAP.md`): for each decision group, list the controlling inputs and how a test sets them:
   | Control | How (verified API) |
   |---|---|
   | DataValidator state | `put(timestamp, val[3] or scalar, error_count, priority)`, `set_timeout()`, `set_equal_value_threshold()`; confidence(t) side effects on flags |
   | Group topology | `DataValidatorGroup(n)`, `add_new_validator()`, `put(index, …)`; current best established by a first `get_best()` |
   | Params | fixture `param_control_autosave(false)` + `param_reset_all()`; `param_set(param_find("FD_…"), &v)` **before** constructing FailureDetector/FailureInjector |
   | uORB inputs | `uORB::Publication<T>`/`PublicationMulti<T>` publish **before** the first `update()`; latest-sample semantics |
   | Vehicle state | `vehicle_status_s`/`vehicle_control_mode_s` structs passed directly to `FailureDetector::update()` |
   | Time | real `hrt_absolute_time()` (CLOCK_MONOTONIC since boot, no lockstep); sleep ≥ 1.5× the documented minimum trigger time |
4. **Reachability & infeasibility proofs** (write them now; they feed GAPS later). Pre-identified (verify each):
   - DVG L213 `best != nullptr` False: when L207's K (`pre_check_prio != -1`) is True, `best` was assigned in the same block (L162–168) → False infeasible.
   - DVG L88 `!validator` True: POSIX `operator new` throws instead of returning null → needs fault injection (optional GCC-only technique) → environment-limited.
   - FI L43 first operand False (`param_get(...) != PX4_OK`): `SYS_FAILURE_EN` is compiled into the parameter table → infeasible in this build.
   - FD L194 `copy()` False inside `updated()` True, FD L222 `copy()` False: investigate; likely infeasible in single-threaded tests.
5. **CFG sketches** (mermaid) for `DataValidator::confidence`, `DataValidatorGroup::get_best`, `FailureDetector::updateMotorStatus`
   → `work/basis/CFG_*.md`. Mark back edges, early exits (`break`, `continue`, `return`) and "not reached ≠ False" spots.
6. **Plan test IDs per decision** (the phase P07–P09 catalogues are the starting point; add rows where a decision outcome has no test).

## Gate G05 checklist
- [ ] 100 % of in-scope decisions listed and verified against the source
- [ ] every outcome has ≥ 1 planned test ID or an infeasibility/environment note
- [ ] SETUP_MAP complete; CFG sketches for the 3 functions
## Explain-back
Walk through `get_best()`'s CFG aloud; explain why the inner switch decision is only evaluated per loop iteration; explain two infeasible outcomes with proofs.
