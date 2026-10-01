# Remaining coverage gaps (format SPEC_10 §4) — every uncovered item of evidence/coverage/final/scope.info must appear here
Pre-identified (verify, then keep/close):
### G-01 DataValidatorGroup.cpp:88 `if (!validator)` True (and the -fcheck-new null edges at L55/L86)
Class: environment-limited · Why: POSIX `operator new` throws; only a non-throwing allocator (NuttX) returns null · Reach: SPEC_02 §9 fault injection (GCC) or on-target test.
### G-03 DataValidatorGroup.cpp:213 `best != nullptr` False
Class: infeasible · Proof: evaluated only when DVG-D15 is True ⇒ K True ⇒ `_curr_best ≥ 0` ⇒ loop 1 set `best` (L168) before any path to L213.
### G-04 FailureInjector.cpp:43 first operand False (`param_get(...) != PX4_OK`)
Class: infeasible in this build · Proof: SYS_FAILURE_EN defined in src/lib/systemlib/system_params.c → `param_find` valid → `param_get` succeeds.
### G-05 exception / compiler-generated edges (if the installed lcov cannot filter them)
Class: tool-artefact · list each file:line and gcov branch index.
### G-07 FailureDetector.cpp:194 / :222 `copy()` False after `updated()` True
Class: to investigate (likely infeasible single-threaded).
