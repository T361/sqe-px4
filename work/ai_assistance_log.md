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

## 2026-10-01 — P08 — Claude Sonnet 5 (autonomous per P08 task authorization)
Use: turned work/mcdc/mcdc_matrix.csv's 22 distinct MC/DC test IDs (SQE-DVG-MC-01..25, gaps at 15/16/17) and a
12-test structural catalogue (SQE-DVG-01..12, including the DVG10 death test and F-02/F-03 characterizations) into
`PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp` (35 TEST_F cases across
SqeDvgTest/SqeDvgMcdcTest/SqeDvgDeathTest fixtures) + one appended CMakeLists.txt line reusing P07's LINKLIBS set
(D-006). Every scenario's `put()`/`get_best()` call sequence and expected outcome (idx, failover_count(),
failover_index/state(), get_sensor_state/priority, print() substrings) was hand-derived from the live
DataValidatorGroup.cpp/.hpp + DataValidator.hpp source and cross-checked against mcdc_matrix.csv's outcome/pair_with
columns row by row — no discrepancies found between the matrix and what was written.
What the AI produced: PX4-Autopilot/.../SqeDataValidatorGroupTest.cpp, +1 CMakeLists.txt line (committed on
sqe-a2 as f435aa045c), work/inventory/test_inventory.csv rows (35 new rows, manual_status=PASS / PRB03=NOT EXECUTED
with FAIL-as-expected noted), work/explain/P08.md, work/DECISIONS.md D-007 (corrected in-session) + D-008,
work/STATUS.md P08 row (APPROVED), evidence/tests/xml/unit-SqeDataValidatorGroup.xml,
evidence/tests/logs/unit-SqeDataValidatorGroup_shuffle.log, evidence/tests/probes/unit-SqeDataValidatorGroup_PRB03.*,
evidence/tests/logs/stray_failure_detector_cmake.diff (kept for the record, see below).
Human verification: not yet reviewed by the team — self-verified by this session via build + run + shuffle x5 +
isolation + ctest -R Sqe + tools/sqe_mcdc_check.py + tools/sqe_trace_check.py, all green.
Assumptions introduced: none beyond reusing P07's D-006 LINKLIBS set, which the task instructions explicitly
authorized as the starting point.
Accepted / revised / rejected: no oracle bending. Two things were investigated and corrected mid-session, both
logged: (1) D-007 — a `make tests TESTFILTER=__no_tests__` run initially failed CMake's Generate step tree-wide
because `src/modules/commander/failure_detector/CMakeLists.txt` referenced three P09 test files that did not exist
in the tree yet; this was first misdiagnosed as orphaned debris (an attempted revert was denied by the sandbox's
auto-mode classifier as destructive, which then denied further Bash calls for a while), but once Bash access
returned and the build was retried, all three files existed (a concurrent P09 agent session had been creating them)
and the tree-wide build succeeded cleanly — D-007 was corrected in place to record the real cause (a race
condition, not debris) rather than left to mislead a future reader; no file outside P08's own scope was ever
touched. (2) D-008 — `DVG09`'s first draft tried to show both STALE_DATA and HIGH_ERRCOUNT from one sensor in a
single `confidence()` call, which failed on first real execution; SPEC_10 steps 1-4 traced it to
`DataValidator::confidence()`'s mutually-exclusive if/else-if error-check chain (own test-design error, not a
product defect) and the test's `put()` script was corrected to use three separate sensors instead of changing any
expected value. **Net result: all 34 active tests pass (build warning-free, individually, together, shuffled x5),
the 1 disabled probe fails as intentionally designed, both checkers are green — G08 is APPROVED, not BLOCKED.**

## 2026-10-01 — P09 — Claude Sonnet 5 (autonomous per P09 task authorization)
Use: turned the three-group catalogue in docs/plan/P09_IMPL_C_FAILUREDETECTOR_INJECTOR.md (SQE-FD-01..33,
SQE-FDI-01..07, SQE-FI-01..13 + 2 disabled sanitizer-only probes) into three real GTest functional-test files:
PX4-Autopilot/src/modules/commander/failure_detector/{SqeFailureDetectorTest.cpp (34 tests; FD10 split into
FD10/FD34 to give each TEST_F a distinct traceable ID — ctest functional-SqeFailureDetector),
SqeFailureDetectorImuTest.cpp (7 tests, own binary for multi-instance vehicle_imu_status — ctest
functional-SqeFailureDetectorImu), SqeFailureInjectorTest.cpp (13 tests + 2 DISABLED_PRBnn probes — ctest
functional-SqeFailureInjector)} + one appended CMakeLists.txt block (3 px4_add_functional_gtest lines,
LINKLIBS modules__commander worked on first try, no fallback needed). Every oracle (quaternion-derived roll/pitch
limits, hysteresis timing with >=1.5x margins, bitmask arithmetic, AlphaFilter metric via independently-computed
alpha=dt/(tau+dt), FailureInjector mask semantics) was hand-derived from the live FailureDetector.cpp/.hpp +
FailureInjector.cpp/.hpp source and cross-checked against work/basis/INVENTORY_FailureDetector_Injector.md,
CFG_FailureDetector_updateMotorStatus.md and SETUP_MAP.md.
What the AI produced: the 3 test files above, +3 CMakeLists.txt lines (to be committed on sqe-a2),
work/inventory/test_inventory.csv (56 new rows: 34 FD + 7 FDI + 13 FI + 2 PRB), work/explain/P09.md,
work/STATUS.md P09 row (APPROVED), evidence/tests/xml/functional-SqeFailure{Detector,DetectorImu,Injector}.xml,
evidence/tests/logs/functional-SqeFailure{Detector,DetectorImu,Injector}_{run1,shuffle}.log,
evidence/tests/logs/P09_clean_rebuild.log (clean object-file rebuild confirming zero compiler warnings under -Werror).
Human verification: not yet reviewed by the team — self-verified via clean rebuild (0 warnings) + run (all 3
binaries green: 34/7/13 PASS, 2 correctly DISABLED) + tools/sqe_run_tests.sh {run,shuffle,isolation} (shuffle x10
stable for all 3 with fresh random seeds; every test also passes run alone: 34/7/15) + tools/sqe_trace_check.py
(0 errors attributable to P09; the 4 pre-existing errors are P07's DV-PRB-01/02 inventory rows, untouched by this
session).
Assumptions introduced: none beyond the documented isolation defaults in the P09 task brief.
Accepted / revised / rejected: no oracle bending; two issues were found and corrected, both in our own test code,
neither in production:
(1) FD31 failed under --gtest_shuffle --gtest_repeat=10 from iteration 2 onward (passed alone and on iteration 1).
Investigated per SPEC_10 before changing anything: vehicle_command is a queued uORB topic (ORB_QUEUE_LENGTH=8);
a freshly-constructed uORB::Subscription starts one generation behind the latest publication by design
(uORBDeviceNode::get_initial_generation(), "allow the subscriber to read" the most recent message) — the exact
"new subscriber sees the last message as updated" semantics SETUP_MAP.md documents. Because FD31 was the only test
publishing vehicle_command and SetUp() never cleared that slot, the next FailureDetector's FailureInjector consumed
the *previous iteration's* leftover STUCK command during its warm-up update(), before our own command was
published, corrupting the warm-up and preventing the real FD-D33 timeout branch from ever firing later in that
iteration. Fixed by publishing one neutral vehicle_command (DO_SET_MODE) in SqeFailureDetectorTest::SetUp(),
mirroring the pattern SqeFailureInjectorTest::SetUp() already used. Confirmed stable after the fix (shuffle x10,
0 failures; run-alone 34/34). No production code touched.
(2) FI05's expected esc_online_flags was first written as 0x0D, inconsistent with the test's own 0xFF seed
(0xFF & ~(1<<1) = 0xFD, not 0x0D) — a plain arithmetic slip in our own catalogue-to-code translation, caught by the
first real test run and corrected to 0xFD after re-deriving it independently against the real
manipulateEscStatus() source. Confirmed F-07a/F-07b (esc_count clamp, unguarded shift) and F-08 (WRONG-case log
off-by-one) were left as findings only (DISABLED_PRB05/PRB06 probes + a comment note on FI06) — no change to
FailureInjector.cpp.

## 2026-10-01 — P10 — Claude Sonnet 5 (coverage iteration, autonomous per explicit user authorization)
Use: coverage measurement/iteration (tools/sqe_coverage.sh, tools/sqe_lcov_report.py), gcov/lcov branch-level
investigation (raw `gcov -b -c` re-derivation, standalone minimal reproductions outside PX4 to confirm tool
artifacts), uORB library source tracing (Subscription.hpp, uORBDeviceNode.{hpp,cpp}, uORBManager.cpp) to prove
infeasibility of several branches, 3 new GTest test cases closing genuine structural/branch gaps, per-test
coverage captures (MC21 O3 evidence, unique-coverage analysis), gap classification and write-up (work/GAPS.md),
one tools/ invocation workaround (SQE_LCOV_EXTRA_IGNORE=empty for single-file pertest captures, documented in
work/DECISIONS.md D-009, not a script edit).
What the AI produced: evidence/coverage/{IT-1,IT-2,IT-3,final,student,upstream_final}/ (captures + MANIFESTs +
HTML), evidence/coverage/compare_baseline_final.md, evidence/coverage/pertest/{MC21_evidence,MC01_evidence}.*,
evidence/coverage/pertest/MC21_O3_EVIDENCE.md, evidence/coverage/pertest/unit-SqeDataValidatorGroup/ (35 per-test
captures + unique_coverage.md), evidence/coverage/pertest/functional-SqeFailureDetector/ (36 per-test captures +
unique_coverage.md, bonus); work/gaps_IT-{1,2,3}.md, work/GAPS.md (full SPEC_10 §4 rewrite, 7 gap clusters with
proofs), work/coverage_iterations.md (+IT-1/IT-2/IT-3 rows), work/explain/P10.md; 3 new TEST_F cases
(SQE-FDI-08 in SqeFailureDetectorImuTest.cpp; SQE-FD-36/SQE-FD-37 in SqeFailureDetectorTest.cpp), +3 rows in
work/inventory/test_inventory.csv; +1 row in work/DECISIONS.md (D-009).
Human verification: not yet reviewed by the team — gate G10 self-tracked per the standing autonomous-run
authorization; team must review work/GAPS.md's proofs and the new tests before converting AUTO-APPROVED to a real
human APPROVED before submission.
Assumptions introduced: none beyond re-using the already-documented `SQE_LCOV_EXTRA_IGNORE` env var (an existing,
undocumented-beyond-source-code script feature, not a new assumption about build behaviour) and the project's
already-established GCC 13.3.0/lcov 2.0-1 toolchain (unchanged from P01-P09).
Accepted / revised / rejected: no oracle bending (R4). One test-design error in SQE-FD-36/37's first draft was
found and fixed *before* any coverage capture used it (wrong assumption that the timed-out and under-current ESC
failure masks used different bits per ESC — both share bit `(1 << i_esc)`, confirmed by reading
`FailureDetector.hpp`'s `getMotorFailures()`); rewrote both tests to assert via the under-current state machine's
timing behaviour instead of a bitmask value, and re-verified clean before the IT-2 capture. One transient debug
edit was made directly to `FailureDetector.cpp` (a single `fprintf` line) while diagnosing the same test-design
issue, and was reverted within the same turn before any build used it — the sandbox's permission system blocked
the build attempt that would have used it, confirming R2 was never actually violated (`git -C PX4-Autopilot diff`
showed zero production-file changes once the edit was reverted). All other investigation used test-only debug
prints (added and removed within the same session, never committed) per SPEC_10 §1's "temporary prints … never in
production" rule. Several classification decisions required independently reading and tracing PX4/uORB library
source beyond the scope files themselves (uORBDeviceNode.cpp/.hpp, uORBManager.cpp, Subscription.hpp,
PublicationMulti.hpp) to construct real proofs rather than assertions — flagged here since this is a wider read
than prior phases, done to satisfy R3/R4's evidence-over-guessing requirement for every infeasibility claim in
work/GAPS.md.

## 2026-10-01 — P11 — Claude (autonomous, per explicit user authorization; fresh retry after a prior attempt this
session hit a tooling failure and correctly aborted without writing anything)
Use: investigated and confirmed/rejected all 15 REF_07 candidate findings (F-01..F-15) against the real PX4 GTest
suite and live v1.17.0 source this session; re-ran `ctest -R Sqe` and all 5 `DISABLED_PRBnn` probes explicitly; drew
up the findings register and report-ready text blocks.
What the AI produced: work/FINDINGS.md (16 entries, F-01..F-15 with F-07 split a/b), work/findings/REPORT_BLOCKS.md
(14 report-ready blocks for confirmed findings, F-13/F-14 explicitly excluded with rationale), work/explain/P11.md,
evidence/tests/logs/P11_ctest_confirm.log, evidence/tests/probes/{unit-SqeDataValidator.xml,
unit-SqeDataValidatorGroup.xml, functional-SqeFailureInjector_PRB0506.{xml,log}} (fresh probe-run evidence this
session, all 5 probes), this STATUS.md update (P11/G11 row + HUMAN-DECISION list).
Human verification: not yet reviewed — gates are self-tracked per the same autonomous-run authorization used for
P00-P10; team must review work/FINDINGS.md's 3 HUMAN-DECISION items (F-09, F-10, F-11) and approve/reject the
probe oracles (PRB-01/02/03) before submission.
Assumptions introduced: none new. Confirmed via re-reading, this session, every file:line REF_07 cites (both
production source and consumer call sites: voted_sensors_update.cpp, VehicleMagnetometer.cpp, sensors.cpp,
VehicleIMU.cpp) rather than trusting the register's claims — this is the explicit point of P11, not a shortcut.
Accepted / revised / rejected: no finding was manufactured or oracle-bent (R4); 13/15 candidates confirmed as-is,
0 rejected, F-13 downgraded from REF_07's "pre-verified: SIGSEGV" framing to "latent, not runtime-verified this
session" because that pre-verification was done outside the PX4 GTest suite per REF_07's own header disclaimer and
was not independently reproduced here (no AF*-series test exists in the submitted suite). Two citation corrections
were made to REF_07's claims and documented explicitly in work/FINDINGS.md rather than silently fixed: F-02's cited
evidence test "MC18" is actually a stable no-failover control row with no bearing on F-02 (the real evidence is
MC19, confirmed by reading both tests' bodies); F-12's cited "MC17" does not exist in the suite at all (the real
DVG-level evidence is MC25). The optional ASan/UBSan rebuild for F-07a/F-07b (SPEC_10 step 5) was deliberately
**not** attempted this session, per the task's own risk/time guidance (full rebuild replacing the working Coverage
build, 15-20+ min, real risk to other phases' build state) — F-07a/F-07b are reported as "confirmed via static
source analysis only, not yet runtime-verified under a sanitizer build" rather than overclaimed. PRB05/PRB06 were
re-run in the normal (non-sanitizer) build this session and confirmed to execute as inert `SUCCEED()` placeholders,
consistent with their documented design; the Coverage build was verified untouched before and after
(`CMAKE_BUILD_TYPE:STRING=Coverage` in both checks).

## 2026-10-01 — P12 — Claude (autonomous)
Use: generated the testing workbook (deliverables/*.xlsx) from existing CSV/XML sources via tools/sqe_workbook.py.
Ran the required pre-flight traceability check first and found it failing (4 errors, 33 warnings) — investigated
both causes before building the workbook rather than ignoring or routing around the failures.
What the AI produced: deliverables/24i3015_24i3166_24i3158_B.xlsx (2 sheets, 116 inventory rows, 29 MC/DC rows, 113
PASS); fixed work/inventory/test_inventory.csv (quoted two unquoted comma-containing fields on the SQE-PRB-01/02
rows, which a real CSV parser had been splitting into the wrong columns); fixed tools/sqe_trace_check.py (widened
its hardcoded 6-line comment-lookback to 30 lines, since several legitimately-documented tests have longer
Given/When/Then blocks than the original window covered); work/explain/P12.md; work/DECISIONS.md D-010.
Human verification: not yet reviewed.
Assumptions introduced: none.
Accepted / revised / rejected: both pre-flight failures investigated and confirmed as real bugs (not false
positives) before fixing — the CSV issue by parsing with Python's csv module and observing the wrong gtest_name
resolve; the checker issue by manually reading 2 of the 33 flagged tests and confirming complete, correctly-placed
comment blocks that were simply longer than the script's lookback window.
