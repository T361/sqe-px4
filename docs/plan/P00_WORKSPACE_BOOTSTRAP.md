# P00 — Workspace bootstrap
**Goal:** a reproducible workspace with living documents, evidence folders and team identity. **Owner:** ORCH. **Gate:** G00.

## Entry
Kit unpacked so that `CLAUDE.md` is in the workspace root (e.g. `~/sqe-a2/CLAUDE.md`).

## Steps
1. `bash tools/sqe_bootstrap.sh` — creates `work/`, `evidence/{env,baseline,build,tests/xml,coverage,mcdc,gates,tmp}`, `deliverables/`,
   copies `templates/work/*` → `work/`, `templates/inventory/*` → `work/inventory/`, `templates/mcdc/*` → `work/mcdc/`,
   and writes a workspace `.gitignore` that ignores `PX4-Autopilot/` and bulky evidence (`*.gcda`, html is kept).
2. Ask the humans for team data; fill `work/TEAM.md` (names, roll numbers, section). Compute the base name
   `<Roll1_Roll2_Roll3_Section>` and write it on the first line as `BASE=...` (used by packaging scripts).
3. Record the chosen platform path in `work/DECISIONS.md` (D-001): Ubuntu 22.04 / Ubuntu 24.04 / WSL2 (Ubuntu 24.04) / macOS (Intel/Apple Silicon).
4. Optional but recommended: `git init` the workspace (not the PX4 clone) and commit `work/` + `docs/` regularly — gives an audit trail.
5. Initialise `work/STATUS.md` (phase table G00–G15 = `TODO`) and `work/ai_assistance_log.md` (first entry: "kit prepared with AI assistance; plan authored by Claude; team reviewed").

## Artifacts
`work/TEAM.md`, `work/STATUS.md`, `work/DECISIONS.md`, `work/ai_assistance_log.md`, `work/production_change_log.md` ("No changes" line).

## Gate G00 checklist
- [ ] `BASE=` line present and matches the LMS naming convention
- [ ] platform decision D-001 recorded
- [ ] evidence/ tree exists
## Explain-back (work/explain/P00.md)
What each folder is for; why evidence is separated from working notes; how gates protect you in the viva.
