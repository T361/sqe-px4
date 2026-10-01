# SPEC_06 — Report specification (binding)
Concise and evidence-focused. Do **not** duplicate test source, workbook rows or raw tool output; reference them. Target 3500–5000 words
excluding appendix. Template: `templates/report/REPORT_TEMPLATE.md`.

| § | Section | Must contain | Words |
|---|---|---|---|
| 1 | Group details & baseline | names, roll numbers, section; tag `v1.17.0`, commit `d6f12ad…` (commit, not tag object) | 80 |
| 2 | Local environment & setup evidence | OS/version, arch, CPU/RAM, compiler(s), cmake, ninja, python, lcov/gcov versions; setup script used; build/test commands; baseline `make tests` summary; pre-existing failures (if any) | 250 |
| 3 | Repository analysis & scope selection record | per area A–D: file/class, responsibility/critical behaviour, dependencies/state, why non-trivial/included (call sites), PX4 test level + justification; exclusions + reasons; candidates rejected (short) | 700 |
| 4 | Testing approach | how obligations were derived (inventories → catalogue), oracle sources, isolation/time control, characterization vs probes, independence evidence (shuffle/repeat); pointer to workbook | 400 |
| 5 | MC/DC component & interpretation | why the component/behaviour is critical; the 5 decisions; coupling/feasibility; 2–3 pairs explained in prose (e.g. D13-E, D15-K with O3, D32-P with timestamp 0); unique-cause vs masking; tool cross-check if done | 700 |
| 6 | Coverage analysis | baseline vs final per file (prose, lines + branches), what the suite contributed (student-only capture), raw vs feasible, gcov branch semantics, iteration history, annotated excerpts | 700 |
| 7 | Findings | confirmed defects (title, location, reproduction, expected, actual, test ID, evidence, severity rationale); observations/specification ambiguities; rejected candidates (1 line each) | 600 |
| 8 | Gaps, limitations, residual risk, improvements | each remaining gap (location, why, what would reach it); environment limits; risks outside scope; concrete improvements (test levels, simulator, dependency control, instrumentation, testability refactoring) | 500 |
| 9 | Final quality judgment | **300–400 words**, between `<!-- JUDGMENT-START -->` and `<!-- JUDGMENT-END -->` | 300–400 |
| 10 | AI-assistance record | material uses + assumptions introduced (SPEC_08) | 200 |
| A | Appendix: reproduction | exact commands from clone to final HTML | — |
| B | Appendix: evidence index | paths of every artefact referenced | — |

## Style rules
- Every number is copied from a file under `evidence/` and cited (`evidence/coverage/final/per_file.md`).
- Say "the analysed scope" — never generalise to PX4 as a whole. No "fully tested", no "bug-free".
- Distinguish: test condition vs test case; failure vs defect; raw vs feasible coverage; characterization vs specification-backed oracle.
- Use course terms: coverage obligations, not reached ≠ False, NE, independence pair, masking, infeasible (with proof), oracle.
- Figures/excerpts: short, annotated, with a caption naming the evidence file.

## Final judgment guidance (§9)
Answer, in this order: what the structural evidence supports about the four files (coverage + MC/DC + oracles); what remains unsupported
(gaps, specification ambiguities, untested interactions with callers such as VotedSensorsUpdate/Commander, hardware timing, NuttX allocator
behaviour); how much confidence the coverage provides (condition-level branch coverage + MC/DC on the voter ⇒ high confidence that each
implemented decision outcome was exercised with a checked oracle — not that requirements are complete); which limitations prevent a broader
conclusion (unit/functional isolation, no SITL/HITL, single compiler/OS, float-rounding sensitivity).
