# P12 — Testing workbook (.xlsx, ≤ 2 sheets)
**Goal:** the compact structured evidence the assignment requires. **Owner:** DOC. **Gate:** G12. **Spec:** SPEC_05 (binding schema).

## Sources of truth
- `work/inventory/test_inventory.csv` — one row per student-authored test (maintained by IMPL as tests are written).
- `evidence/tests/xml/*.xml` and `evidence/tests/probes/*.xml` — execution results (never typed by hand).
- `work/mcdc/mcdc_matrix.csv` — MC/DC rows (from P06, updated with executed test IDs).

## Steps
1. `python3 tools/sqe_trace_check.py --px4 PX4-Autopilot --inventory work/inventory/test_inventory.csv --mcdc work/mcdc/mcdc_matrix.csv`
   → must report 0 missing / 0 extra / 0 orphan MC/DC references.
2. `python3 tools/sqe_mcdc_check.py work/mcdc/mcdc_matrix.csv` → `ALL DECISIONS COMPLETE`.
3. `python3 tools/sqe_workbook.py --inventory work/inventory/test_inventory.csv --mcdc work/mcdc/mcdc_matrix.csv --xml evidence/tests/xml evidence/tests/probes --out deliverables/<BASE>.xlsx`
   (reads `BASE` from `work/TEAM.md` if `--out` is omitted). The script fills *Execution Result* from XML, validates the schema, and fails on
   inconsistencies (e.g. a row without a result, > 2 sheets).
4. Open the file in Excel/LibreOffice/Google Sheets once; check wrapping/filters; do not add sheets. If edited in Google Sheets, export .xlsx again.

## Gate G12 checklist
- [ ] exactly 2 sheets: "Test Inventory", "MC-DC Evidence" (a sheet name cannot contain "/")
- [ ] row count Sheet 1 = number of student tests (incl. probes and characterization tests)
- [ ] every MC/DC decision shows each condition T and F, the decision T and F, a pair per condition
