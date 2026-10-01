# STATUS — SQE A2 (update every session)
Baseline: PX4-Autopilot v1.17.0 @ d6f12ad1c4f70ad3230afd7d86e971421e02fef4 · branch `sqe-a2`
Build type in use: Coverage (since P03) · Last full test run: 2026-10-01 (ctest -R Sqe: 5/5 binaries passed, incl.
unit-SqeDataValidatorGroup 34/34) · Current phase: P08 complete, P09 in progress (parallel)

| Phase | Gate | Status (TODO / IN-PROGRESS / READY-FOR-HUMAN / APPROVED / BLOCKED) | Evidence | Approved by / date |
|---|---|---|---|---|
| P00 Bootstrap | G00 | APPROVED | work/, evidence/, deliverables/ created; TEAM.md complete | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P01 Env + clone | G01 | APPROVED | evidence/env/baseline_commit.txt, evidence/env/environment.md | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P02 Baseline tests | G02 | APPROVED | evidence/baseline/make_tests.log, ctest_list.txt, LastTest_baseline.log, summary.txt — 147/147 (100%) passed, O-01 confirmed | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P03 Coverage baseline | G03 | APPROVED | evidence/coverage/baseline/ (MANIFEST, scope.info w/ BRF>0 for all 4 files, html/, per_file.md) — DataValidator 74.1%/70.0% L/B, DataValidatorGroup 65.2%/46.6%, FailureDetector/Injector 0%/0% (expected, no upstream test starts Commander) | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P04 Scope | G04 | APPROVED (pending human review of D-004) | work/scope/SCOPE_RECORD.md, work/scope/CANDIDATES.md, work/scope/upstream_gtests.txt | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P05 Test basis | G05 | APPROVED | work/basis/ (3 inventories + SETUP_MAP + 3 CFG sketches, all verified against live source) | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P06 MC/DC | G06 | APPROVED | work/mcdc/MCDC_ANALYSIS.md, work/mcdc/mcdc_matrix.csv (29 rows, 22 planned test IDs, 5 decisions), work/mcdc/mcdc_check_report.md — `tools/sqe_mcdc_check.py` reports ALL DECISIONS COMPLETE (0 errors) | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P07 Impl A DataValidator | G07 | APPROVED | PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorTest.cpp (20 SQE-DV tests + 2 DISABLED_PRB probes, ctest `unit-SqeDataValidator`); CMakeLists.txt +2 lines (LINKLIBS fallback, see D-006); evidence/tests/xml/unit-SqeDataValidator.xml (20/20 PASS, 2 NOT EXECUTED); evidence/tests/logs/unit-SqeDataValidator_shuffle.log (shuffle x5 stable); evidence/tests/logs/P07_build_attempt4.log (clean warning-free build); `tools/sqe_trace_check.py` clean; work/inventory/test_inventory.csv rows added; work/explain/P07.md. Coverage % not measured this session — evidence/coverage/ was in concurrent use by another process (baseline/ present); deferred to P10, not blocking. | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P08 Impl B DataValidatorGroup | G08 | APPROVED | PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp (35 TEST_F: structural DVG01-12 incl. death test DVG10, MC/DC MC01-25 = 22 distinct test IDs per work/mcdc/mcdc_matrix.csv, 1 DISABLED probe PRB03), +1 CMakeLists.txt line (LINKLIBS reused from D-006), committed PX4-Autopilot f435aa045c; evidence/tests/xml/unit-SqeDataValidatorGroup.xml (34/34 PASS, 1 DISABLED); evidence/tests/logs/unit-SqeDataValidatorGroup_shuffle.log (x5 stable, all 34 PASS each repeat); isolation check (35/35 tests pass run alone); `ctest -R Sqe` 5/5 binaries pass tree-wide; `tools/sqe_mcdc_check.py work/mcdc/mcdc_matrix.csv` → ALL DECISIONS COMPLETE; `tools/sqe_trace_check.py` → 0 errors for SQE-DVG*/SQE-PRB-03 rows; probe PRB03 executed explicitly and FAILS as expected (confirms F-02); work/inventory/test_inventory.csv rows added (manual_status=PASS); work/explain/P08.md; one test-design error found and fixed during verification (DVG09, see work/DECISIONS.md D-007) | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P09 Impl C FailureDetector/Injector | G09 | APPROVED | PX4-Autopilot/src/modules/commander/failure_detector/{SqeFailureDetectorTest.cpp (34 tests, ctest `functional-SqeFailureDetector`), SqeFailureDetectorImuTest.cpp (7 tests, `functional-SqeFailureDetectorImu`), SqeFailureInjectorTest.cpp (13 tests + 2 DISABLED_PRB probes, `functional-SqeFailureInjector`)}; CMakeLists.txt +3 lines (LINKLIBS modules__commander, no fallback needed); evidence/tests/xml/functional-SqeFailure{Detector,DetectorImu,Injector}.xml (34/7/13 PASS, 2 NOT EXECUTED as designed); evidence/tests/logs/functional-SqeFailure{Detector,DetectorImu,Injector}_shuffle.log (shuffle x10 stable, 0 failures across all 3) + `tools/sqe_run_tests.sh isolation` (34/7/15 pass run alone) + evidence/tests/logs/P09_clean_rebuild.log (clean rebuild, zero compiler warnings under -Werror); `tools/sqe_trace_check.py` clean (0 errors attributable to P09); work/inventory/test_inventory.csv rows added (56); work/explain/P09.md. Coverage % not measured this session (deferred to P10, same as P07/P08). One test-isolation bug found and fixed in our own fixture (FD31 leftover queued vehicle_command across --gtest_repeat iterations — see work/explain/P09.md); one arithmetic slip in our own FI05 oracle corrected (0x0D -> 0xFD). F-07a/F-07b/F-08 left as findings, not patched. | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P10 Coverage iteration | G10 | TODO | evidence/coverage/final/ | |
| P11 Findings | G11 | TODO | work/FINDINGS.md | |
| P12 Workbook | G12 | TODO | deliverables/*.xlsx | |
| P13 Report | G13 | TODO | deliverables/report/ | |
| P14 Packaging | G14 | TODO | deliverables/ | |
| P15 Viva | G15 | TODO | work/viva/ | |

## Blockers
- none currently. (Resolved during P08: the first `make tests TESTFILTER=__no_tests__` run of this session hit a
  CMake Generate failure tree-wide — `src/modules/commander/failure_detector/CMakeLists.txt` referenced
  `SqeFailureDetectorTest.cpp`/`SqeFailureDetectorImuTest.cpp`/`SqeFailureInjectorTest.cpp`, none of which existed
  in the tree yet at that moment. This was initially misdiagnosed as orphaned debris (see the now-superseded
  investigation in work/DECISIONS.md D-007) — it was actually a **race condition with a concurrent P09 agent
  session** that was creating those exact files at the same time; by the time the build was retried a few minutes
  later all three files existed and the tree-wide configure succeeded cleanly. No file was reverted or altered
  outside P08's own scope. See D-007 for the full corrected account.)

## HUMAN-DECISION items (open)
- D-004 exclusions list (P04) · probe oracles PRB-01/02/03 (P11) · classification of F-09/F-10/F-11 (P11)

## Next steps
1. P09 (FailureDetector/FailureInjector, in progress concurrently — its test files/CMake lines exist in the
   PX4-Autopilot worktree but were not committed by P08 and are outside this phase's scope).
2. P10 coverage iteration, including capturing the MC21 O3 per-test evidence (line 210 not-executed vs MC01) that
   was deferred from P08.
