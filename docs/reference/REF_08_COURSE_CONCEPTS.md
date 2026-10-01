# REF_08 — Course concepts (SE3002 lectures 2–8) mapped to this assignment
| Concept (lecture) | How it appears here | Where you must show it |
|---|---|---|
| Test basis → test condition → test case; oracle (L3) | inventories (basis) → catalogue rows (conditions) → gtest cases with exact expected results + oracle source | SPEC_02 §3, workbook col 5 |
| PASSED / FAILED / BLOCKED / NOT EXECUTED (L3) | workbook "Execution Result" from XML; probes FAIL by design; sanitizer-only probes NOT EXECUTED in normal runs | SPEC_05 |
| Failed test ≠ defect; causes: product, oracle, setup, environment, automation, requirement (L3) | SPEC_10 protocol before any classification | P11 |
| Defect report fields (L3) | title, environment, preconditions, steps, expected, actual, reproducibility, severity, evidence, related test | SPEC_10 §2 |
| Test levels; component vs integration; drivers/stubs (L4) | unit (A/B), functional (C/D, real uORB/params runtime), SITL not needed; uORB publications act as drivers | SCOPE_RECORD, report §3 |
| CFG, basic blocks, back edges, early exits (L5) | CFG sketches for confidence/get_best/updateMotorStatus | P05 step 5 |
| Statement coverage formula; scripts/global state are inputs (L5) | lcov lines; hidden state (`_curr_best`, masks, uORB last sample) controlled explicitly | SPEC_03 §5, SPEC_02 §6 |
| Branch/decision coverage; "not reached ≠ False"; loops; multi-way decisions (L6) | gcov edges incl. loop exits, switch cases, ternaries; inner decisions of get_best reached only in loops | report §6 |
| Infeasible outcomes: prove, keep "feasible coverage" separate from the raw tool % (L6) | G-03 (L213), G-04 (FI L43) proofs | GAPS.md |
| Coverage ≠ oracle quality; missing functionality; data-position/boundary defects (L6) | F-09 (missing NaN handling), F-10 (boundary), F-06 (quantisation) | report §7–§8 |
| Atomic conditions, condition coverage, condition+decision, short-circuit NE (L7) | gcov per-operand edges; `NE(x)` in Sheet 2 | SPEC_04 §1 |
| MCC per decision (2ⁿ), feasibility, domain-valid vs code-feasible (L8) | why not MCC for D13 (2⁷ = 128 vectors, many infeasible); MC17 uses a domain-invalid timestamp | report §5 |
| MC/DC: independence pairs, unique-cause vs masking (next block after L8) | REF_06 pairs; coupling F⇒D, B⇒G | Sheet 2, report §5 |
| GenAI audit: derive → generate → verify → execute → audit → decide (L5–L8) | every AI-drafted artefact audited by a student + auditor agent; AI record | SPEC_08, work/audit/ |
