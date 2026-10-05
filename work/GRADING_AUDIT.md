# Independent Grading Audit — SE3002 A2 (PX4 Structural Testing)
Grader: adversarial TA pass, 2026-10-05. Read-only; nothing in the repo was modified by this audit
beyond building/running `unit-SqeDataValidatorAllocFault` for verification (explicitly permitted).

> **STATUS: historical record, all findings addressed.** Every item in "Red flags" below (#1-#4) was fixed
> immediately after this audit landed, same session: `work/GAPS.md` G-01 rewritten to reflect the real state;
> `REPORT.md` §6/§9/§4/Appendix B updated with correct coverage numbers (100.0% line, was 99.8%) and test counts
> (119 inventory rows, 6 binaries, 113 active); `AF02`'s comment's crash-site misattribution corrected
> (`prev->setSibling(next)`, not `_first->get_timeout()`) and the test rebuilt/re-verified; the workbook, patch,
> and zip regenerated against the current committed state; the AllocFault commit itself landed on PX4-Autopilot's
> `sqe-a2` branch. Kept here unedited as the honest paper trail of what the audit found and when — do not read
> the body below as describing the current state of the repository.

## Executive summary
This is a strong, largely self-consistent submission with genuine, independently-reproducible evidence behind
almost every cited number, a real (not performative) MC/DC derivation, and at least one legitimately good
late addition (GCC allocation-fault injection). However, that same late addition (`SqeDataValidatorAllocFaultTest.cpp`,
added this session, outside the frozen deliverable) was never propagated into `work/GAPS.md`, the test
inventory, the workbook, or `REPORT.md` §6/§9 — so the submitted report currently makes a claim ("G-01 ...
never executed") that is no longer true of the live tree, and the submitted `.patch`/`.xlsx`/`.zip` are stale
by one real test file relative to `PX4-Autopilot/` HEAD. Nothing found rises to fabrication; this is an
integration/packaging gap, not a dishonesty problem. Net: a genuinely strong submission held back by late,
unfinished work and one real process violation (G07 build-dir state).

## Verification log (what I actually ran)
- `git -C PX4-Autopilot rev-parse HEAD` → `0b026417397db06635b62a1df46b19b92991af20`
- `git -C PX4-Autopilot merge-base v1.17.0 HEAD` → `d6f12ad1c4f70ad3230afd7d86e971421e02fef4` (exact match to
  the pinned baseline — HEAD is a clean 4-commit fast-forward descendant, correct per R1).
- `git -C PX4-Autopilot diff --stat v1.17.0 HEAD`: exactly 7 files, all `Sqe*Test.cpp` + 2 `CMakeLists.txt`
  (+5/+8 lines each). Zero production `.cpp`/`.hpp` touched. R2 holds for everything *committed*.
- `git -C PX4-Autopilot status --porcelain`: `M src/.../data_validator/CMakeLists.txt`,
  `?? src/.../SqeDataValidatorAllocFaultTest.cpp` — uncommitted, R2-compliant content (CMake registration +
  new `Sqe*Test.cpp`), but **not yet part of the committed/packaged submission**.
- `ctest -R Sqe` (build dir `px4_sitl_test`): **6/6 binaries, 100% passed**, 5.34s wall. The 6th binary,
  `unit-SqeDataValidatorAllocFault`, is new this session and passes (2/2 tests: `AF01`, death-test `AF02`).
- Per-binary `--gtest_list_tests` counts (recounted by hand, not trusted from any doc): FailureDetector 36,
  FailureDetectorImu 8, FailureInjector 15, DataValidator 22 (incl. 2 DISABLED), DataValidatorGroup 35 (incl. 1
  DISABLED), AllocFault 2. **Live total = 118 tests across 6 binaries**, vs. the report/workbook/STATUS's
  cited "116 rows / 111 active + 5 disabled". The gap is exactly the 2 new AF tests, confirming they are real
  and additional, not double-counted or fabricated — just un-integrated.
- `tools/sqe_mcdc_check.py work/mcdc/mcdc_matrix.csv` → `ALL DECISIONS COMPLETE` (0 errors), reproduced.
- Hand re-derivation of 2 MC/DC pairs directly from `DataValidatorGroup.cpp:187-190` (DVG-D13):
  - **MC01/MC02 pair isolating F** (`next->priority() > max_priority`): MC01 (m=.95,mp=50,c=.95,p=75) →
    C=False (.95>.95 false) short-circuits D; falls to E=True(|.95-.95|<0.01), F=True(75>50) → inner True,
    G=True → decision **True**. MC02 (same m/mp/c, p=50) → E=True, F=False(50>50 false) → inner False → decision
    **False**. F flips T→F with every other evaluated condition (E) held constant → valid unique-cause pair.
    Matches `mcdc_matrix.csv` rows exactly.
  - **MC04/MC05 pair isolating D** (`next->priority() >= max_priority`): MC04 (c=.97,p=50 vs mp=50) → C=True,
    D=True(50>=50) → decision **True**. MC05 (c=.97,p=25) → C=True, D=False(25>=50) → falls to E=False(0.02
    not <0.01) → decision **False**. D flips T→F with C held True → valid pair. Matches the matrix.
  - Both hand recomputations match the submitted CSV exactly — the MC/DC matrix is not fabricated or
    hand-waved; the derivation in `work/mcdc/MCDC_ANALYSIS.md` (coupling proofs `F⇒D`, `B⇒G`, `(A∧B)⇒C`,
    independently re-derived with explicit algebra, a documented self-correction on `_prev_best`
    bookkeeping) is substantive, not performative.
- `evidence/coverage/final/per_file.md` read directly: DataValidator 58/58 (100.0%) / 30/30 (100.0%);
  DataValidatorGroup 154/155 (99.4%) / 110/116 (94.8%); FailureDetector 153/153 (100.0%) / 189/254 (74.4%);
  FailureInjector 62/62 (100.0%) / 47/57 (82.5%); total 427/428 (99.8%) / 376/457 (82.3%). These numbers match
  `REPORT.md` §6 and §9 **exactly** — no drift between the report prose and the raw evidence file it cites.
  (Caveat: this evidence predates the AllocFault binary — see Red Flags.)
- `deliverables/24i3015_24i3166_24i3158_B.xlsx`: exactly 2 sheets, "Test Inventory" (116 data rows, 8 cols,
  header matches the PDF's required fields exactly) and "MC-DC Evidence" (29 data rows). No AF rows present —
  confirms staleness, not fabrication (nothing claims AF exists in the workbook).
- `deliverables/24i3015_24i3166_24i3158_B.patch` (141KB, 3351 lines, dated before this session's AllocFault
  work) does not contain "AllocFault" anywhere — confirms the packaged patch is genuinely one file behind
  `PX4-Autopilot/` HEAD, not a copy/paste inconsistency.
- Production scope size sanity check: DataValidator.cpp 155 lines, DataValidatorGroup.cpp 347, FailureDetector.cpp
  354, FailureInjector.cpp 134 (990 total) — this is real, non-trivial sensor-voting/failure-detection control
  logic (fixed-wing/multirotor failure detection, redundant-sensor arbitration), not GUI/trivial/generated code.
  Judged genuinely substantial and meaningful, not cherry-picked for easy 100%.

## Read the new fault-injection test on its own merits (G-01)
`PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorAllocFaultTest.cpp` overrides global
`operator new`/`delete` (GCC-only, gated by `#if defined(__GNUC__) && !defined(__clang__)`, with an honest
`GTEST_SKIP()` fallback on other compilers) to make the next heap allocation return `nullptr` on command. This
is a real, working fault-injection technique exactly matching the instructor's clarification that students
should attempt mocks/fakes/fault-injection rather than default to "infeasible." It is built and run in its own
binary (correctly isolated — a global operator-new override cannot safely share a process with other tests,
and the CMakeLists change correctly gates it to `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"`). `AF01` is a clean,
well-oracled test of `add_new_validator()`'s `if (!validator) return nullptr;` guard (DataValidatorGroup.cpp:88)
— genuinely closes that part of G-01. `AF02` is a death test exercising the **constructor's** first allocation
failing; it passes (confirmed `EXPECT_DEATH` catches a real SIGSEGV), but its own explanatory comment
misattributes the crash mechanism: it claims the crash comes from `_first->get_timeout()` at line 70, but
reading the live source (`DataValidatorGroup.cpp:69-71`) shows that call is guarded by `if (_first) { ... }` —
not reachable with `_first == nullptr`. Tracing the actual control flow for `siblings=2` with the *first*
`new DataValidator()` failing: `_first = next` (null), loop continues to `i=1`, second `new` succeeds, then
`prev->setSibling(next)` executes with `prev` still null — **that** is the real null-pointer dereference, not
line 70. The test and its oracle (EXPECT_DEATH) are still correct; only the prose narrative in the comment
misdiagnoses which exact line crashes. Minor, but worth flagging — it's a (harmless) factual inaccuracy of
exactly the kind the project's own R3/R4 rules say should not survive ("even when harmless to the checker's
pass/fail result"), and this one did.

## G-02 through G-07 spot check (did they genuinely attempt test-doubles before calling things infeasible?)
`work/GAPS.md` G-02 (gcov-synthesized null-guard on `delete`, tool-artefact, backed by a `gcov -b -c` re-run
this session) and G-03 (same `best != nullptr` dead-code claim independently reconstructed in F-01 via explicit
control-flow dominance proof — I traced `pre_check_prio`/`best` assignments myself at
`DataValidatorGroup.cpp:146-213` and the proof holds) are genuine structural/compiler-generated artefacts, not
hand-waved. G-01 is the one gap explicitly flagged as "neither was implemented this session... flagged here
rather than silently dropped" — and that gap was **in fact subsequently attempted and partly closed** by the
AllocFault file, which is good-faith evidence the team (or this session) followed the instructor's directive
rather than defaulting to "infeasible." The problem is purely that `GAPS.md` text was never updated to say so.

## Red flags (stale/inconsistent numbers found this session)
1. **`work/GAPS.md` G-01 is now factually wrong.** It still reads "Neither was implemented this session...
   flagged here rather than silently dropped," and separately "a dedicated `SqeDataValidatorAllocFaultTest.cpp`
   ... is the documented strategy [for later]" — but that exact file now exists, builds, and passes. G-01
   should be (at minimum) reclassified for the `add_new_validator()` null-check branch (AF01 closes it) and
   kept open only for the constructor path pending a correction to AF02's comment.
2. **`deliverables/report/REPORT.md` §9 (the 300-400 word final judgment) makes a claim that is no longer
   true**: "The NuttX non-throwing-allocator path (G-01) ... [was] never executed." This is now false for at
   least the `add_new_validator()` half of G-01. This is the opposite failure mode from overclaiming coverage —
   it's an *under*-claim caused by staleness — but it is still evidence in the final judgment section that
   does not match the live repository state.
3. **Test-count drift, again, after the "109 vs 111" fix was supposedly closed.** Live `ctest -R Sqe` now shows
   6 binaries / 118 tests; `REPORT.md` (line 394, line 489), `STATUS.md`'s header, `work/inventory/test_inventory.csv`
   (116 rows, `grep -c "SQE-"` confirms), and the workbook (116 data rows) all still say 116/111. This is the
   same category of bug D-012 claims was fixed, recurring via a different path (a new file added after the
   last count refresh, not reconciled before this audit).
4. **Packaging drift**: `deliverables/24i3015_24i3166_24i3158_B.patch` (and by extension the `.zip`) predates
   the AllocFault work and does not contain it — so the "official" submission artefacts under `deliverables/`
   are missing a real, passing, student-authored test file and its CMake registration. If this session's work
   is meant to be part of the final submission, packaging must be re-run; if not, the loose files in
   `PX4-Autopilot/` should not be left uncommitted/undocumented without a STATUS.md note (there currently is
   none — `work/STATUS.md`'s "Next steps" section makes no mention of the AllocFault work at all).
5. No placeholder/TODO text, no "100% verified" unbacked claims, and no forbidden "PX4 is high quality/fully
   tested" language were found anywhere in `REPORT.md` — this is a genuine positive, not just an absence of
   evidence of a problem.
6. `work/STATUS.md`'s own "batch sign-off" note is a candid, non-performative admission that G00–G14 were not
   reviewed per-gate as R7 strictly requires — this is honest self-reporting rather than a process violation
   being hidden, and is itself evidence the team/session is not inclined to oversell its own rigor.

## Rubric-style component notes (informal — full numeric grading belongs to the human grader; this audit is a
process/evidence check, not a mark allocation)
- **Repository analysis & scope (PDF 20 marks)**: scope is substantial, business/control logic, correctly
  justified against 4 real files with real call sites cited (`voted_sensors_update.cpp`,
  `VehicleMagnetometer.cpp`). Solid.
- **Structural derivation & MC/DC (PDF 30 marks)**: the strongest part of the submission. 5 compound decisions
  on `DataValidatorGroup`, up to 7 conditions (D13), real algebraic coupling proofs, a documented self-caught
  error in `_prev_best` reasoning, correct O1/O2/O3 observability classification. Two independently
  hand-recomputed pairs both check out exactly against the live source. This is not "two conditions because
  easy" — it is a legitimately hard target handled competently.
- **Test implementation & execution (PDF 15 marks)**: clean, deterministic, Given/When/Then test code (spot
  checked `SqeDataValidatorAllocFaultTest.cpp`); correct isolation of the one test file that needed a separate
  binary; shuffle/repeat x3 confirmed stable for `unit-SqeDataValidatorGroup` in this audit. Docked for the
  packaging/patch staleness noted above.
- **Coverage measurement & gap analysis (PDF 20 marks)**: numbers in the report trace exactly to
  `evidence/coverage/final/per_file.md`, no fabricated rows found. Docked for G-01's gap writeup now being
  stale relative to a real fix, and for the AF02 comment's factual misattribution.
- **Findings & final judgment (PDF 15 marks)**: findings read as genuinely investigated (F-01's dead-code proof
  independently re-verified by this audit against live source), appropriately scoped language, no overclaiming
  of "PX4 is fully tested." Docked slightly because §9 now contains one stale/incorrect sentence (G-01).

## Viva risk
Not directly assessable by this audit (requires live human demonstration per the PDF's rubric). One specific
risk worth flagging to examiners: whoever added `SqeDataValidatorAllocFaultTest.cpp` this session should be
able to correctly explain AF02's actual crash mechanism (`prev->setSibling(next)` on a null `prev`, not the
`_first->get_timeout()` line the test's own comment currently cites) — as written, a student reciting the
comment verbatim would give an examiner a wrong answer.
