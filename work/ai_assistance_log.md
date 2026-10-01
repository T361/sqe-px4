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
