# P13 — Report
**Goal:** one concise, evidence-focused report covering the submission checklist. **Owner:** DOC. **Gate:** G13.
**Spec:** SPEC_06 (outline, word budgets, style) · template `templates/report/REPORT_TEMPLATE.md`.

## Steps
1. Copy the template to `deliverables/report/REPORT.md`; fill sections in this order: §2 environment (from `evidence/env/environment.md`),
   §3 scope (from SCOPE_RECORD), §4 approach, §5 MC/DC (from MCDC_ANALYSIS), §6 coverage (from per_file.md, compare file, GAPS),
   §7 findings (from REPORT_BLOCKS), §8 limitations/improvements, §9 judgment, §10 AI record, appendix reproduction + evidence index.
2. Every number: paste with its evidence path in a comment `<!-- src: evidence/coverage/final/per_file.md -->`; the auditor checks them.
3. Include 2–4 annotated excerpts (screenshot or text) of the lcov HTML for key decisions (e.g. DataValidatorGroup L187–L190 all branches taken).
4. Word counts: `python3 tools/sqe_word_count.py deliverables/report/REPORT.md` (judgment must be 300–400; whole report ideally ≤ 5000).
5. Export: `pandoc deliverables/report/REPORT.md -o deliverables/<BASE>.pdf` (needs a LaTeX engine) **or** export to .docx
   (`pandoc … -o report.docx`) and save as PDF from Word/LibreOffice. Check tables and code blocks render.
6. Auditor pass: `/sqe-audit deliverables/report/REPORT.md` → fix all MAJOR/BLOCKER items.

## Gate G13 checklist
- [ ] all SPEC_06 sections present; no duplicated test code/tool dumps
- [ ] numbers traceable; raw vs feasible coverage distinguished
- [ ] judgment 300–400 words, scoped, no "PX4 is high quality/fully tested"
