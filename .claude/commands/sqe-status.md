---
description: Show SQE A2 status, verify baseline, and propose the next steps on the critical path
---
1. Print the phase table from work/STATUS.md (create it from templates/work/STATUS.md if missing).
2. Run `git -C PX4-Autopilot rev-parse HEAD`, `git -C PX4-Autopilot branch --show-current`, `git -C PX4-Autopilot status --porcelain | head`.
   Confirm `git -C PX4-Autopilot merge-base HEAD v1.17.0` = d6f12ad1c4f70ad3230afd7d86e971421e02fef4.
3. List open BLOCKERs and HUMAN-DECISION items.
4. Propose the next 3–5 steps from docs/plan/00_MASTER_PLAN.md critical path, each with the artifact it produces.
