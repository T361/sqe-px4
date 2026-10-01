# P15 — Viva preparation (every member must understand the complete submission)
**Goal:** each student can build/run any test, locate its production decision, name atomic conditions, explain an independence pair,
justify test levels, interpret a gap, and predict coverage lost if a test is removed. **Owner:** all. **Gate:** G15.

## Materials
- `docs/reference/REF_09_VIVA_QUESTION_BANK.md` (questions + model answers keyed to this scope)
- `work/explain/P*.md` (explain-back notes) · `/sqe-explain <ID>` for any test/decision/gap
- `evidence/coverage/pertest/*/unique_coverage.md` from `tools/sqe_unique_coverage.py` ("coverage lost if removed")

## Drills (log each in `work/viva/drill_log.md` with date, student, pass/fail)
1. **Build & run live** (5 min): `cmake --build … --target unit-SqeDataValidatorGroup` → run `--gtest_filter='*MC04*'` → open the HTML at L187.
2. **Locate & explain** (3 min): examiner picks a random test ID → student names file:line, conditions, the setup lines, the oracle.
3. **MC/DC pair** (3 min): examiner picks a condition (e.g. D of DVG-D13) → student gives the pair (MC04/MC05), the changed input, the logical
   values, why only D changes, how the outcome is observed.
4. **Modify a test** (5 min): change a boundary (e.g. DV-12 to T0+40002, DV-03 threshold to 5) → predict, run, explain.
5. **Remove a test** (3 min): pick a test → predict lost lines/branches → confirm with unique-coverage table.
6. **Gap defence** (3 min): explain L213 infeasibility proof and the L88 environment limitation, and what would be needed to cover them.
7. **Level justification** (2 min): why unit for A/B, functional for C/D, not SITL.
8. **Findings** (3 min): present F-09 or F-10 honestly (what is confirmed, what is a specification question).
Rotate until each student passes every drill on areas they did **not** implement.
