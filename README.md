# SQE A2 — PX4 v1.17.0 Structural Testing Kit (for Claude Code)

This kit turns SE3002 Assignment 02 into an executable, phase-gated plan that Claude Code (or any coding agent) can run end-to-end while your team stays in control and understands every artifact for the viva.

## What is inside
| Path | Purpose |
|---|---|
| `CLAUDE.md` | Rules + session protocol Claude Code loads automatically. Start here. |
| `AGENTS.md` | Agent roles, hand-off contracts, parallelism, mapping to the 3 students. |
| `.claude/agents/` | Claude Code sub-agent definitions (analyst, MC/DC engineer, implementer, coverage, auditor, writer…). |
| `.claude/commands/` | Slash commands: `/sqe-status`, `/sqe-phase`, `/sqe-gate`, `/sqe-audit`, `/sqe-explain`. |
| `docs/plan/` | `00_MASTER_PLAN.md` (critical path, gates) + one file per phase `P00`…`P15`. |
| `docs/specs/` | Binding specifications: IDs/naming, test-code standard, coverage pipeline, MC/DC method, workbook schema, report template, patch/submission, AI record, evidence, investigations. |
| `docs/reference/` | Facts verified against the v1.17.0 source, scope candidate matrix, line-level decision inventories, MC/DC pre-derivation, findings register, course concepts, viva bank, troubleshooting. |
| `templates/` | Working files copied into `work/` by the bootstrap script; C++ test skeletons; CSV schemas; report template. |
| `tools/` | Scripts: bootstrap, environment report, test runner, coverage pipeline, lcov reports, per-test unique coverage, traceability + MC/DC checks, workbook generator, patch builder, word counter. |

## Quick start
```bash
mkdir -p ~/sqe-a2 && cd ~/sqe-a2
unzip /path/to/sqe-a2-px4-kit.zip && mv sqe-a2-px4-kit/* sqe-a2-px4-kit/.claude . && rmdir sqe-a2-px4-kit
bash tools/sqe_bootstrap.sh          # creates work/, evidence/, deliverables/ from templates
claude                               # start Claude Code in ~/sqe-a2 (CLAUDE.md is picked up here)
```
First prompt to Claude Code:
> Read CLAUDE.md and docs/plan/00_MASTER_PLAN.md. Run /sqe-status. Then execute Phase P00 and stop at its gate.

Then drive phase by phase: `/sqe-phase P01`, `/sqe-gate P01`, … Each phase ends with an **explain-back** note written for you; read it before approving the gate.

## What the humans must do (Claude Code cannot)
1. Fill `work/TEAM.md` (names, roll numbers, section) — needed for file naming.
2. Run long builds on your own machine if Claude Code is not running on it (local execution is compulsory).
3. Approve every gate in `work/STATUS.md`; decide the disputed items flagged `HUMAN-DECISION`.
4. Understand everything: any member may be asked to explain/modify any test in the viva. Use `docs/plan/P15_VIVA_PREP.md`.
5. Keep the AI-assistance record honest (`work/ai_assistance_log.md`). The assignment permits AI assistance **with** a record; "the AI said so" is not a defensible test basis.

## Verified baseline (from the actual v1.17.0 source)
- Tag `v1.17.0` (annotated tag object `a5eb12d2ab591251faa009f76b2685b8cc64405d`) → commit **`d6f12ad1c4f70ad3230afd7d86e971421e02fef4`** (2026‑01‑16).
- `git rev-parse HEAD` after `git clone --branch v1.17.0 --recursive …` must print the commit above.

The kit was prepared by static analysis of the v1.17.0 sources; nothing was compiled while writing it. Phase P01–P03 gates therefore make Claude Code re-verify every build/coverage assumption on your machine before any test is written.
# sqe-px4
