# AI-assistance log (append-only; format SPEC_08)
## <date> — P00 — Claude (kit preparation)
Use: plan authoring, repository navigation (static reading of v1.17.0), test-design drafts (catalogues, MC/DC pre-derivation), script drafts
What the AI produced: this kit (docs/, tools/, templates/)
Human verification: <who reviewed which parts, how>
Assumptions introduced: build/coverage commands derived from Makefile/CMake reading (verified on our machine in P02/P03: <yes/no>);
lcov flag names per version; float32 values of confidence thresholds
Accepted / revised / rejected: <…>

## 2026-10-01 — P00+P01 — Claude (orchestration, autonomous per explicit user authorization)
Use: workspace bootstrap, team-data entry, PX4-Autopilot clone/pin verification, blocker diagnosis.
What the AI produced: work/STATUS.md, work/DECISIONS.md, work/TEAM.md, work/explain/P00.md, work/explain/P01.md,
evidence/env/baseline_commit.txt; cloned PX4-Autopilot and verified HEAD = d6f12ad1c4f70ad3230afd7d86e971421e02fef4 on
branch sqe-a2.
Human verification: not yet reviewed by the team — gates are self-tracked by the agent per explicit autonomous-run
authorization given in-session; team must review evidence before converting READY-FOR-HUMAN/AUTO-APPROVED rows to a real
human APPROVED before submission.
Assumptions introduced: platform = Ubuntu 24.04 native (D-001); setup script = Tools/setup/ubuntu.sh --no-nuttx
--no-sim-tools (D-002, not yet executed — needs sudo).
Accepted / revised / rejected: pending human review.
