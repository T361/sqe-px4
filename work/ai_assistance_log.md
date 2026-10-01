# AI-assistance log (append-only; format SPEC_08)
## <date> — P00 — Claude (kit preparation)
Use: plan authoring, repository navigation (static reading of v1.17.0), test-design drafts (catalogues, MC/DC pre-derivation), script drafts
What the AI produced: this kit (docs/, tools/, templates/)
Human verification: <who reviewed which parts, how>
Assumptions introduced: build/coverage commands derived from Makefile/CMake reading (verified on our machine in P02/P03: <yes/no>);
lcov flag names per version; float32 values of confidence thresholds
Accepted / revised / rejected: <…>

## 2026-10-01 — P00+P01 — Claude (orchestration, autonomous per explicit user authorization)
Use: workspace bootstrap, team-data entry, PX4-Autopilot clone/pin verification, blocker diagnosis.
What the AI produced: work/STATUS.md, work/DECISIONS.md, work/TEAM.md, work/explain/P00.md, work/explain/P01.md,
evidence/env/baseline_commit.txt; cloned PX4-Autopilot and verified HEAD = d6f12ad1c4f70ad3230afd7d86e971421e02fef4 on
branch sqe-a2.
Human verification: not yet reviewed by the team — gates are self-tracked by the agent per explicit autonomous-run
authorization given in-session; team must review evidence before converting READY-FOR-HUMAN/AUTO-APPROVED rows to a real
human APPROVED before submission.
Assumptions introduced: platform = Ubuntu 24.04 native (D-001); toolchain installed user-local without sudo (cmake/ninja
via pip --user, lcov/genhtml built from source to ~/.local, Perl DateTime via local::lib/cpan to ~/perl5) instead of
Tools/setup/ubuntu.sh, because non-interactive sudo is unavailable and CLAUDE.md R9 reserves sudo for humans (D-002).
Accepted / revised / rejected: pending human review.

## 2026-10-01 — P01 — Claude (toolchain install workaround, autonomous)
Use: resolved a genuine blocker (no passwordless sudo) by installing cmake, ninja, lcov, genhtml, and the Perl DateTime
module entirely in user space (~/.local, ~/perl5), avoiding the sudo-gated Tools/setup/ubuntu.sh. Generated
evidence/env/environment.md confirming every required tool resolves to a real version and the clone is pinned correctly.
What the AI produced: evidence/env/environment.md, work/explain/P01.md (updated), ~/.bashrc PATH/local::lib additions
(outside the repo, on this machine only).
Human verification: not yet reviewed — team should confirm the user-local toolchain is acceptable for their own
machines too if they reproduce this build elsewhere (the original sudo-based ubuntu.sh script remains the documented
"official" path in P01's instructions for other machines).
Assumptions introduced: none beyond D-002.
Accepted / revised / rejected: pending human review.

## 2026-10-01 — P02+P04+P05(partial) — Claude (autonomous)
Use: ran baseline build/test (`make tests`), extracted/verified evidence; independently re-verified the scope
candidate matrix and all three pre-derived decision inventories (REF_03/04/05) line-by-line against the live v1.17.0
source rather than trusting them; wrote SCOPE_RECORD.md and CANDIDATES.md from that verification.
What the AI produced: evidence/baseline/{make_tests.log,ctest_list.txt,LastTest_baseline.log,summary.txt},
work/explain/P02.md, work/scope/{upstream_gtests.txt,SCOPE_RECORD.md,CANDIDATES.md},
work/basis/INVENTORY_{DataValidator,DataValidatorGroup,FailureDetector_Injector}.md (copied from REF_03/04/05 with
added verification notes).
Human verification: not yet reviewed. One correction made during verification: REF_02 claimed `Safety.cpp`/
`UserModeIntention.cpp` had upstream tests ("yes (dir)") — re-checked, found no dedicated gtest registration for
either, corrected in CANDIDATES.md (doesn't change the rejection verdict, just the stated reason).
Assumptions introduced: none new.
Accepted / revised / rejected: REF_02's Safety.cpp/UserModeIntention.cpp upstream-test claim revised (see above); all
other pre-derived content (REF_02 candidate metrics, REF_03/04/05 decision inventories, reachability proofs,
flagged defects F-07/F-08) independently confirmed accurate against the live source, accepted as-is.

## 2026-10-01 — P07 — Claude (autonomous, DataValidator GTest implementation)
Use: turned the pre-derived P07 test catalogue (docs/plan/P07_IMPL_A_DATAVALIDATOR.md, SQE-DV-01..20 + 2 probes)
into real GTest code. Independently re-derived every catalogue oracle by hand (Python arithmetic worked out from
the live DataValidator.cpp/.hpp source, not copied from the production expressions) before writing any assertion,
per R4 — all 20 catalogue oracles checked out exactly, including the three characterization edge cases (DV-07
all-NaN stream, DV-15 density-exactly-at-window, DV-20 timestamp-0 re-init). No catalogue correction was needed.
What the AI produced: PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorTest.cpp (20 TEST_F cases
+ 2 DISABLED_PRBnn probes, fixture SqeDataValidatorTest); one appended CMakeLists.txt block (px4_add_unit_gtest);
work/inventory/test_inventory.csv (22 rows); work/explain/P07.md; work/DECISIONS.md D-006 (LINKLIBS resolution —
the P07 doc's first-choice `modules__sensors` and its own documented fallback both failed to link with undefined
references to px4_log_raw/px4_task_spawn_cmd/etc.; resolved by reusing the exact library set px4_add_gtest.cmake's
px4_add_functional_gtest() macro already auto-links for the same reason, since DataValidator.cpp's print()/hrt
usage pulls in the full posix platform stack even for a "pure" unit test).
Verification performed: build is warning-free under -Werror (evidence/tests/logs/P07_build_attempt4.log,
P07_build_clean_warncheck.log); 20/20 tests pass individually and as a suite (evidence/tests/xml/
unit-SqeDataValidator.xml); --gtest_shuffle --gtest_repeat=5 stable (evidence/tests/logs/
unit-SqeDataValidator_shuffle.log, also tools/sqe_run_tests.sh shuffle); the 2 DISABLED probes fail as intended
when run explicitly via tools/sqe_run_tests.sh probes (confirms R4 compliance — no oracle was bent to pass);
tools/sqe_trace_check.py reports 20 PASS + 2 NOT EXECUTED with no errors.
Human verification: not yet reviewed — gate self-tracked per explicit autonomous-run authorization in-session.
Assumptions introduced: coverage percentage for DataValidator.cpp under this test file was not measured in this
session (evidence/coverage/ showed a baseline/ directory from concurrent in-session work; task instructions said
not to touch it) — deferred to P10, flagged in STATUS.md, not treated as a blocker for G07.
Accepted / revised / rejected: all 20 SQE-DV oracles and both probe oracles accepted as correctly derived from the
live source; no defects found in the catalogue itself.

## 2026-10-01 — P03 — Claude (autonomous)
Use: reconfigured build to Coverage type, ran the coverage pipeline's baseline capture (all 147 upstream tests under
coverage instrumentation).
What the AI produced: evidence/build/coverage_build.log, evidence/coverage/baseline/* (MANIFEST, scope.info,
per_file.md, html/), work/explain/P03.md, work/coverage_iterations.md IT-0 row, work/DECISIONS.md D-005.
Human verification: not yet reviewed.
Assumptions introduced: none.
Accepted / revised / rejected: found and reverted 2 files unintentionally modified by the EKF2 test suite's own
golden-output regeneration as a side effect of running under Coverage build flags (not our edit, not R2-allowed) —
see D-005.

## 2026-10-01 — P06 — Claude (autonomous)
Use: independently re-derived the MC/DC matrix for DataValidatorGroup's 5 compound decisions (DVG-D13/D14/D15/D32/D34)
from the live v1.17.0 source (re-read `DataValidatorGroup.cpp` L140-311, `DataValidatorGroup.hpp`, `DataValidator.hpp`,
`DataValidator.cpp` this session), writing short standalone Python scripts mirroring C++'s exact short-circuit
evaluation order to compute logical values/traces/outcomes by hand-equivalent arithmetic for every scenario, then
cross-checked against `docs/reference/REF_06_MCDC_PREDERIVATION.md`'s pre-derivation. Verified the three coupling
claims (F⇒D, B⇒G, (A∧B)⇒C) algebraically and the DVG-D16/DVG-D14 infeasibility proofs by re-reading the relevant
source lines directly. Found and corrected one explanatory error introduced during derivation (an incorrect
assumption about `_prev_best` bookkeeping on the first-ever `get_best()` call, corrected after re-reading
`DataValidatorGroup.cpp:218-224` — did not change any matrix row's T/F content, only the notes' explanation).
What the AI produced: work/mcdc/MCDC_ANALYSIS.md, work/mcdc/mcdc_matrix.csv (29 rows / 22 planned test IDs / 5
decisions, all unique-cause pairs, one O3 row documented), work/mcdc/mcdc_check_report.md (tool output),
work/explain/P06.md.
Human verification: not yet reviewed — `tools/sqe_mcdc_check.py work/mcdc/mcdc_matrix.csv` passes with
"ALL DECISIONS COMPLETE" (0 errors, 0 warnings) as of this session; team should re-run it themselves and spot-check
a few rows' arithmetic (e.g. DVG-D13's MC07/MC08 pair on condition A) before relying on it for P08.
Assumptions introduced: none beyond re-using REF_06's scenario design pattern (seed-then-introduce-candidate via two
`get_best()` calls) where it was independently confirmed correct; all numeric inputs, traces, and infeasibility
proofs were recomputed from scratch, not copied.
Accepted / revised / rejected: REF_06's §1-§4 content independently reproduced and accepted as accurate (no
discrepancies found); one own-derivation error (the `_prev_best` bookkeeping assumption above) caught and revised
before finalizing, not present in REF_06 to begin with.
