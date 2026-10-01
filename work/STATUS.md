# STATUS — SQE A2 (update every session)
Baseline: PX4-Autopilot v1.17.0 @ d6f12ad1c4f70ad3230afd7d86e971421e02fef4 · branch `sqe-a2`
Build type in use: <Debug|Coverage> · Last full test run: <date> · Current phase: P00

| Phase | Gate | Status (TODO / IN-PROGRESS / READY-FOR-HUMAN / APPROVED / BLOCKED) | Evidence | Approved by / date |
|---|---|---|---|---|
| P00 Bootstrap | G00 | APPROVED | work/, evidence/, deliverables/ created; TEAM.md placeholders | AUTO-APPROVED (autonomous run per user direction 2026-10-01) |
| P01 Env + clone | G01 | BLOCKED (sudo password needed) | evidence/env/baseline_commit.txt | |
| P02 Baseline tests | G02 | TODO | evidence/baseline/ | |
| P03 Coverage baseline | G03 | TODO | evidence/coverage/baseline/ | |
| P04 Scope | G04 | TODO | work/scope/ | |
| P05 Test basis | G05 | TODO | work/basis/ | |
| P06 MC/DC | G06 | TODO | work/mcdc/ | |
| P07 Impl A DataValidator | G07 | TODO | evidence/tests/ | |
| P08 Impl B DataValidatorGroup | G08 | TODO | evidence/tests/ | |
| P09 Impl C FailureDetector/Injector | G09 | TODO | evidence/tests/ | |
| P10 Coverage iteration | G10 | TODO | evidence/coverage/final/ | |
| P11 Findings | G11 | TODO | work/FINDINGS.md | |
| P12 Workbook | G12 | TODO | deliverables/*.xlsx | |
| P13 Report | G13 | TODO | deliverables/report/ | |
| P14 Packaging | G14 | TODO | deliverables/ | |
| P15 Viva | G15 | TODO | work/viva/ | |

## Blockers
- **P01 toolchain install needs sudo password.** `PX4-Autopilot/Tools/setup/ubuntu.sh --no-nuttx --no-sim-tools` installs
  cmake, ninja-build, lcov, genhtml, gcc/g++ deps, and Python requirements via `apt-get`. Non-interactive sudo failed
  (`sudo: a password is required` — no passwordless sudo policy on this host). Per CLAUDE.md R9, sudo may only be run by
  the humans. **Action needed:** a human runs, in this repo's `PX4-Autopilot/` directory:
  `sudo bash ./Tools/setup/ubuntu.sh --no-nuttx --no-sim-tools`
  then confirms back here (or just re-runs the agent) so the env report (`tools/sqe_env_report.sh`) and the rest of P01 can
  complete. Clone itself is done and verified (see below) — only the toolchain install is blocked.

## HUMAN-DECISION items (open)
- D-004 exclusions list (P04) · probe oracles PRB-01/02/03 (P11) · classification of F-09/F-10/F-11 (P11)

## Next steps
1. …
