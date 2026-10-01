# SPEC_08 — AI-assistance record (binding)
The assignment permits AI assistance if material uses and assumptions are recorded; raw chat transcripts are **not** submitted unless requested.

## Log entry format (`work/ai_assistance_log.md`, append-only)
```
## <date> — <phase> — <tool: Claude Code / model>
Use: repository navigation | build troubleshooting | test scaffolding | code suggestion | analysis draft | writing draft
What the AI produced: <1–2 lines, with artefact paths>
Human verification: <who checked what, how (re-derived from source, re-ran, reviewed diff)>
Assumptions introduced: <e.g. "assumed AlphaFilter α = dt/(τ+dt)" → verified in AlphaFilter.hpp>
Accepted / revised / rejected: <decision + reason>
```
## Report §10 (≤ 200 words)
Summarise by category (navigation, build troubleshooting, scaffolding, derivation drafts, writing), name the verification practice (every test
traced to a source line, every MC/DC pair recomputed by a student, every number re-run), and list the key assumptions the AI introduced and how
they were checked. State explicitly that the team takes responsibility for all targets, expected results, tests and conclusions.
