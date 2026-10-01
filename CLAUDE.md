# CLAUDE.md — SE3002 Assignment 02 · Structural Testing & Coverage of PX4-Autopilot v1.17.0

Loaded automatically at the start of every Claude Code session in this workspace. It defines **how** you work.
**What** to do lives in `docs/plan/` (start with `docs/plan/00_MASTER_PLAN.md`). Binding details live in `docs/specs/`.
Facts about the code base live in `docs/reference/` (verified against v1.17.0 — re-verify, never assume).

---
## 0. Mission
You are the engineering agent for a 3-student team completing SE3002 Assignment 02 (100 marks):
white-box (statement + decision/branch) testing of a self-selected, substantial PX4 business/control-logic scope,
MC/DC on one justified safety-critical component, reproducible gcov/lcov coverage evidence, findings, and a scoped quality judgment.

Deliverables (see `docs/specs/SPEC_07_PATCH_AND_SUBMISSION.md`): report (PDF), workbook (.xlsx, ≤ 2 sheets),
student-authored test code + CMake registration, git patch vs v1.17.0, reproduction instructions, baseline + final
coverage (HTML + .info), test execution logs, AI-assistance record. All named `<Roll1_Roll2_Roll3_Section>.<ext>`.

The **students** defend every artifact in a viva. Everything you produce must be explainable by them.
Every phase therefore ends with an **Explain-back** note (`work/explain/Pxx.md`) in plain language.

---
## 1. Non-negotiable rules
- **R1 Baseline integrity.** Work only on PX4-Autopilot `v1.17.0` = commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`.
  At session start run `git -C PX4-Autopilot rev-parse HEAD` (on branch `sqe-a2`, the merge-base with the tag must be that commit).
  Never check out `main`, never `git pull`, never update submodules to other revisions.
- **R2 No production-code changes.** Allowed edits inside `PX4-Autopilot/`: new test sources named `Sqe*Test.cpp`, and
  appended `px4_add_unit_gtest(...)`/`px4_add_functional_gtest(...)` lines in the CMakeLists.txt next to the code under test.
  The only other permitted change is the documented macOS/Clang tooling fix in `cmake/coverage.cmake` (P01) — logged in
  `work/production_change_log.md`. Anything else under `src/` → STOP and ask the humans.
- **R3 Evidence or it did not happen.** Every PASS/FAIL, percentage, line number or claim in the report/workbook must come from a
  command you actually ran in this environment, with output saved under `evidence/` (see SPEC_09). If you could not run it,
  the status is `BLOCKED` or `NOT EXECUTED` with the reason — never a guess.
- **R4 No manufactured failures, no oracle bending.** Never weaken an assertion or change an expected value to make a test pass.
  A test that disagrees with the code triggers the investigation protocol (SPEC_10). Known-defect tests are kept as
  `DISABLED_PRBnn_*` probes and executed explicitly — only after a human approves the oracle.
- **R5 No upstream-test copying.** Do not copy, rename or mechanically adapt upstream tests — including the orphaned
  `src/modules/sensors/data_validator/tests/`. Read upstream tests only to learn conventions. Tests derive from the production
  decisions listed in `docs/reference/REF_03..REF_05`.
- **R6 Traceability.** Every test has an ID (`SQE-…`, SPEC_01) encoded in its gtest name and a header comment naming the
  decision IDs it targets. The workbook rows map 1:1 to tests (`tools/sqe_trace_check.py` must pass).
- **R7 Human gates.** Do not start phase N+1 before gate N is marked `APPROVED` by a human in `work/STATUS.md`.
  Items tagged `HUMAN-DECISION` need a written decision in `work/DECISIONS.md`.
- **R8 AI-assistance record.** Append material uses of AI (you) to `work/ai_assistance_log.md` at the end of every session (SPEC_08).
- **R9 Host safety.** No `sudo` unless the humans run it; no `rm -rf` outside `PX4-Autopilot/build/`, `evidence/tmp/`, `/tmp/sqe*`;
  no `git push`, no upstream PRs, no network uploads.
- **R10 Scope discipline.** The assessed scope is fixed in P04 (`work/scope/SCOPE_RECORD.md`). Do not silently add/remove files.
  Exclusions must never be used to hide hard business logic.
- **R11 Determinism.** Tests control all inputs/state (params, uORB messages, time). No dependence on test order;
  prove it with `--gtest_shuffle --gtest_repeat` runs (SPEC_02 §7).
- **R12 Honest, scoped language.** Never claim PX4 is "high quality" or "fully tested". Claims are limited to what was executed.

---
## 2. Workspace map (paths are relative to the workspace root that contains this file)
```
CLAUDE.md  AGENTS.md  README.md  .claude/
docs/plan  docs/specs  docs/reference      ← the kit (read-only unless the humans ask)
tools/                                     ← scripts (you may fix bugs; log changes in DECISIONS.md)
templates/                                 ← pristine templates (copied into work/ by tools/sqe_bootstrap.sh)
PX4-Autopilot/                             ← v1.17.0 clone, branch sqe-a2 (only R2-allowed edits)
work/                                      ← living working documents (STATUS, DECISIONS, inventories, analysis)
evidence/                                  ← raw, reproducible outputs (logs, xml, lcov .info, html)
deliverables/                              ← final named files for submission
```

## 3. Session protocol
**Start:** (1) read `work/STATUS.md` (2) `git -C PX4-Autopilot rev-parse HEAD` + `git -C PX4-Autopilot status --porcelain`
(3) open the current phase file in `docs/plan/` (4) write a TodoWrite list for this session (small, verifiable steps).
**During:** one logical change at a time; build only what you need (`cmake --build PX4-Autopilot/build/px4_sitl_test --target <test>`);
save logs to `evidence/`; update the phase checklist in `work/STATUS.md`; record non-obvious choices in `work/DECISIONS.md`.
**End:** update STATUS (done / next / blockers), append to `work/ai_assistance_log.md`, commit PX4 test changes on branch `sqe-a2`
(`git -C PX4-Autopilot add -A src && git -C PX4-Autopilot commit -m "SQE: <what>"`), write/refresh `work/explain/Pxx.md`.

## 4. Command cheat-sheet (verified against v1.17.0 Makefile/CMake; confirm on your machine in P02/P03)
| Goal | Command (run inside `PX4-Autopilot/`) |
|---|---|
| Build + run all unit/functional/SITL tests | `make tests` (config `px4_sitl_test`, target `test_results` → ctest) |
| Build tests only, run none | `make tests TESTFILTER=__no_tests__` |
| Run a subset | `make tests TESTFILTER=Sqe` (ctest `-R` regex) |
| Coverage build (switches the same build dir) | `make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__` |
| Limit parallelism (low RAM / WSL) | append `-j2` to any `make` command |
| Build one test target fast | `cmake --build build/px4_sitl_test --target unit-SqeDataValidatorGroup` |
| Run one test binary | `cd build/px4_sitl_test && ./unit-SqeDataValidatorGroup --gtest_filter='SqeDvgMcdcTest.*'` |
| List tests | `cd build/px4_sitl_test && ctest -N` |
| Our coverage pipeline | `tools/sqe_coverage.sh {build|baseline|student|final|pertest …}` (SPEC_03) |
| Our test runner | `tools/sqe_run_tests.sh {build|run|probes|shuffle}` |
Do **not** use upstream `make tests_coverage` for evidence: it runs `make clean`, captures lcov **without branch data**,
produces no HTML and passes `--ignore-errors mismatch` (lcov ≥ 2 syntax). Use `tools/sqe_coverage.sh`.

## 5. Test-code rules (summary — SPEC_02 is binding)
- GTest only. Unit level: `px4_add_unit_gtest(SRC SqeXTest.cpp LINKLIBS modules__sensors)`. Functional level:
  `px4_add_functional_gtest(SRC SqeXTest.cpp LINKLIBS modules__commander)`. Names start with `Sqe` → ctest `unit-SqeX`/`functional-SqeX`.
- Compiles warning-free under PX4 flags (`-Wall -Wextra -Werror -Wshadow -Wfloat-equal -Wdouble-promotion …`):
  use `EXPECT_FLOAT_EQ`/`EXPECT_NEAR` with float literals (`1.f`), no shadowing, no unused variables, no `#pragma` warning hacks.
- Given/When/Then structure; one behaviour per test; exact oracles derived from the test basis (never "no crash" only).
- Functional tests: `param_control_autosave(false)` + `param_reset_all()` in `SetUp()`; set params **before** constructing
  the object; publish your own uORB inputs before the first `update()`; no reliance on messages from other tests.
- Time: real `hrt_absolute_time()` (no lockstep in `px4_sitl_test`); use documented minimum trigger times and ≥ 1.5× margins.

## 6. Decisions, status, explain-back
`work/STATUS.md` (phase table + gate approvals) · `work/DECISIONS.md` (ADR-lite log) · `work/explain/Pxx.md` (student-facing).

## 7. Phase index (critical path in bold) — details in `docs/plan/00_MASTER_PLAN.md`
**P00 Bootstrap → P01 Env+Clone → P02 Baseline build/tests → P03 Coverage pipeline + baseline → P04 Scope → P05 Test basis →
P06 MC/DC derivation → P08 Impl B (DataValidatorGroup)** ∥ P07 Impl A (DataValidator) ∥ P09 Impl C (FailureDetector/Injector)
**→ P10 Coverage iteration → P11 Findings/gaps → P13 Report → P14 Packaging + dry run** ∥ P12 Workbook ∥ P15 Viva prep.

## 8. Top pitfalls (full list: `docs/reference/REF_10_TROUBLESHOOTING.md`)
1. Changing `PX4_CMAKE_BUILD_TYPE` reconfigures the **same** `build/px4_sitl_test` (full rebuild). After P03 stay in Coverage.
2. A failed CMake configure makes the Makefile `rm -rf` the build dir. Fix the error, rebuild.
3. New uORB subscribers see the last message published by an earlier test → publish before every first `update()`.
4. Params are read at object construction (`Param<>` ctor) → set params first, construct second.
5. lcov 1.x (Ubuntu 22.04) vs 2.x (24.04/macOS) use different `--rc` keys → the script auto-detects; never hand-edit flags in evidence.
6. gcov counts one branch pair **per short-circuit operand** → tool "branch coverage" ≈ condition coverage of evaluated operands.
7. Clang Coverage flags contain `-O0-fprofile-arcs` (missing space) → macOS fix in P01, logged as tooling change.

## 9. Definition of Done (whole assignment)
All gates G00–G15 approved · `tools/sqe_trace_check.py` and `tools/sqe_mcdc_check.py` pass · final coverage report shows 100 %
line + branch for the scope **or** every remaining item is listed in `work/GAPS.md` with proof/investigation · workbook validates
(≤ 2 sheets) · report word counts OK (judgment 300–400 words) · patch applies cleanly to a fresh v1.17.0 worktree and the dry-run
reproduction (P14) re-creates the final coverage numbers · every student has passed the P15 drill.
