# AGENTS.md — Roles, hand-offs and parallelism

Applies to Claude Code sub-agents (`.claude/agents/*.md`, invoked through the Task tool) and to any other coding agent
(Codex, Cursor, …) that reads `AGENTS.md`. All agents obey `CLAUDE.md` §1 (R1–R12). When rules conflict, `CLAUDE.md` wins.

## 1. Roles
| Code | Role | Owns phases | Writes | Never does |
|---|---|---|---|---|
| ORCH | Orchestrator / lead | all gates, P00, P14 | `work/STATUS.md`, `work/DECISIONS.md` | approve a gate on behalf of humans |
| ENV | Environment & build engineer | P01, P02 | `evidence/env/`, `evidence/baseline/` | modify `src/` |
| COV | Coverage engineer | P03, P10 | `evidence/coverage/`, `work/coverage_iterations.md`, `work/GAPS.md` (draft) | edit tests to "game" coverage |
| ANL | Code analyst | P04, P05 | `work/scope/`, `work/basis/` | invent decisions not in the source |
| MCDC | MC/DC engineer | P06 | `work/mcdc/` | fabricate infeasible rows as tests |
| IMPL | Test implementer | P07, P08, P09 | `PX4-Autopilot/src/**/Sqe*Test.cpp`, CMake lines, `work/inventory/` | touch production code |
| AUD | Independent auditor (read-only) | every gate | `work/audit/Gxx.md` | fix what it audits (it reports) |
| DOC | Writer | P11 (prose), P12, P13, P15 | `deliverables/`, `work/explain/` | state numbers not present in `evidence/` |

## 2. Hand-off contracts (artifact → acceptance criteria)
| From → To | Artifact | Accepted when |
|---|---|---|
| ENV → COV | `evidence/env/environment.md`, `evidence/baseline/make_tests.log`, `ctest_list.txt` | HEAD verified; upstream tests executed; failures (if any) classified pre-existing |
| COV → ANL | `evidence/coverage/baseline/` (scope.info, html/, per_file.md, MANIFEST.md) | baseline numbers per scope file present; tool versions recorded |
| ANL → MCDC/IMPL | `work/scope/SCOPE_RECORD.md`, `work/basis/INVENTORY_*.md`, `work/basis/SETUP_MAP.md` | every decision line in scope listed with outcomes, conditions, setup; line numbers re-verified |
| MCDC → IMPL | `work/mcdc/mcdc_matrix.csv`, `work/mcdc/MCDC_ANALYSIS.md` | `tools/sqe_mcdc_check.py` passes; every row maps to a planned test ID |
| IMPL → COV | test sources + CMake lines committed on `sqe-a2`; `work/inventory/test_inventory.csv`; `evidence/tests/*.xml` | all tests compile warning-free, pass (or probes fail as documented), shuffle/repeat stable |
| COV → DOC | `evidence/coverage/final/`, `work/GAPS.md`, `work/coverage_iterations.md` | every uncovered item classified + justified |
| AUD → ORCH | `work/audit/Gxx.md` | zero open `BLOCKER` findings |
| DOC → ORCH | `deliverables/*` | SPEC_06/07 checklists ticked; word counts OK |

## 3. Parallelism (safe concurrent work)
- After G04: P07 (DataValidator tests) may start while P05/P06 continue (it has no MC/DC dependency).
- After G05: P09 (FailureDetector/Injector) may run in parallel with P06 → P08.
- P12 (workbook) can be generated incrementally as soon as the first tests pass; P15 drills start once P08 is green.
- Never run two builds of `build/px4_sitl_test` at once (Ninja lock + gcda races). Serialize all `make`/`cmake --build` calls.
- Coverage captures must run with no other test process writing `.gcda` files.

## 4. Mapping to the three students (viva ownership — everyone must still know everything)
| Student | Primary | Secondary |
|---|---|---|
| S1 | Environment, baseline, coverage pipeline, gap analysis (ENV, COV) | FailureDetector tests |
| S2 | DataValidator + DataValidatorGroup, MC/DC (ANL, MCDC, IMPL-A/B) | Workbook |
| S3 | FailureDetector/Injector functional tests (IMPL-C), findings (SPEC_10) | Report, packaging |
Each student presents their area in the P15 mock viva and answers cross-questions on the others' areas.

## 5. Escalation
Stop and write `BLOCKER:` in `work/STATUS.md` when: HEAD ≠ baseline; a production edit seems necessary; a build/coverage step fails
twice after the REF_10 fix; a test result contradicts the oracle and SPEC_10 steps 1–4 do not explain it; the scope needs to change.

## 6. Invoking sub-agents in Claude Code
Use the Task tool with the agent names in `.claude/agents/` (e.g. "use the sqe-auditor agent to audit gate G06").
Give each sub-agent: the phase file path, the exact artifacts to read/write, and the acceptance criteria from §2.
