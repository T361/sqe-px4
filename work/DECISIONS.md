# DECISIONS (ADR-lite; append-only)
| ID | Date | Decision | Alternatives considered | Rationale / evidence | Decided by |
|---|---|---|---|---|---|
| D-001 | 2026-10-01 | Platform path: Ubuntu 24.04 (native) | Ubuntu 22.04 / WSL2 / macOS | `lsb_release -a` = Ubuntu 24.04.4 LTS, native host, 12 cores, 72G free disk | ORCH (autonomous) |
| D-002 | 2026-10-01 | Setup script command: PX4 `Tools/setup/ubuntu.sh` (run once, logged to evidence/env/) | manual package install | standard PX4 bootstrap for Ubuntu | ORCH (autonomous) |
| D-003 | 2026-10-01 | Scope areas A–D as in REF_02 (DataValidator, DataValidatorGroup, FailureDetector, FailureInjector) | battery, land detector, … | pre-selected by static analysis in README/master plan; re-verified in P04 | ORCH (autonomous) |
| D-004 | 2026-10-01 | Exclusions: header-only accessors; legacy upstream tests (read-only, not copied per R5); `print()`/debug dump methods kept out of MC/DC scope (no decision logic) | including trivial accessors in scope | trivial getters have no branches; would inflate test count without testing logic | ORCH (autonomous; flagged HUMAN-DECISION in STATUS, user has pre-authorized autonomous operation) |
