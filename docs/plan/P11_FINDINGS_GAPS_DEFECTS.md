# P11 — Findings, failures/blockers investigation, gaps, defects
**Goal:** every FAIL/BLOCKED investigated; each candidate finding classified with evidence; nothing overclaimed. **Owner:** AUD + DOC.
**Gate:** G11. **Spec:** SPEC_10 (protocol + templates). **Register:** REF_07 (F-01…F-15 pre-identified candidates).

## Steps
1. List every non-PASS result from `evidence/tests/xml/*.xml` (`python3 tools/sqe_trace_check.py --results`).
2. For each: run SPEC_10 steps 1–6 (setup → expected result → state/data → environment → dependency behaviour → reproduce 3×) and write
   `work/findings/INV-nn.md`. Only after steps 1–6 may you classify: product defect / wrong oracle / setup error / environment / automation
   defect / requirement-spec ambiguity.
3. Walk through REF_07 candidates F-01…F-15: confirm or reject each with the named evidence test; record the verdict in `work/FINDINGS.md`.
   HUMAN-DECISION required for: probe oracles (PRB-01/02/03), classification of F-09, F-10, F-11 (defect vs specification ambiguity).
4. Probes: after approval, `tools/sqe_run_tests.sh probes` runs `DISABLED_PRB*` tests explicitly (`--gtest_also_run_disabled_tests`) and stores
   XML/logs in `evidence/tests/probes/`. Expected result FAIL; the workbook shows the real status.
5. Optional sanitizer evidence for F-07 (and a general memory-safety pass):
   ```bash
   cd PX4-Autopilot && make tests PX4_ASAN=1 TESTFILTER=Sqe 2>&1 | tee ../evidence/findings/asan_run.log     # full rebuild (not Coverage!)
   (cd build/px4_sitl_test && ./functional-SqeFailureInjector --gtest_also_run_disabled_tests --gtest_filter='*PRB05*' ) 2>&1 | tee ../evidence/findings/asan_prb05.log
   make tests PX4_UBSAN=1 TESTFILTER=Sqe ... ; run *PRB06*
   ```
   Sanitizer builds replace the Coverage build (same directory). Re-run `tools/sqe_coverage.sh build` afterwards before any further coverage capture.
6. Finalise `work/GAPS.md` (from P10) with the proofs; every gap has: location, why unreachable/impractical here, what would reach it.
7. Draft report §7–§8 text blocks in `work/findings/REPORT_BLOCKS.md` (defect format: title, source location, reproduction conditions,
   expected, actual, test ID, evidence path, severity rationale).

## Gate G11 checklist
- [ ] zero un-investigated non-PASS results
- [ ] each REF_07 candidate has verdict + evidence path
- [ ] no finding described as a defect without reproduction + justified oracle
## Explain-back
Tell the story of one confirmed finding end to end (test → failure/observation → investigation → classification) and one rejected candidate.
