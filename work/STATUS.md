# STATUS — SQE A2 (update every session)
Baseline: PX4-Autopilot v1.17.0 @ d6f12ad1c4f70ad3230afd7d86e971421e02fef4 · branch `sqe-a2`
Build type in use: Coverage (since P03) · Last full test run: 2026-10-01 (147/147 passed, under Coverage instrumentation) · Current phase: P06/P07 (parallel)

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
| P08 Impl B DataValidatorGroup | G08 | TODO | evidence/tests/ | |
| P09 Impl C FailureDetector/Injector | G09 | TODO | evidence/tests/ | |
| P10 Coverage iteration | G10 | TODO | evidence/coverage/final/ | |
| P11 Findings | G11 | TODO | work/FINDINGS.md | |
| P12 Workbook | G12 | TODO | deliverables/*.xlsx | |
| P13 Report | G13 | TODO | deliverables/report/ | |
| P14 Packaging | G14 | TODO | deliverables/ | |
| P15 Viva | G15 | TODO | work/viva/ | |

## Blockers
- none currently. (Resolved: P01's toolchain install originally needed `sudo apt-get` via `Tools/setup/ubuntu.sh`, which
  needs a password this environment can't supply. Worked around with a user-local install instead — see D-002 — so no
  human action was needed after all.)

## HUMAN-DECISION items (open)
- D-004 exclusions list (P04) · probe oracles PRB-01/02/03 (P11) · classification of F-09/F-10/F-11 (P11)

## Next steps
1. …
