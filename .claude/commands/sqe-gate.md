---
description: Run the verification commands of a gate and report pass/fail with evidence (e.g. /sqe-gate G08)
argument-hint: <gate id, e.g. G08>
---
Find gate $ARGUMENTS in docs/plan/00_MASTER_PLAN.md §4 and in its phase file. Run every verification command listed, capture output
to evidence/gates/$ARGUMENTS/, and print a table: check | command | result | evidence path. Do not modify artifacts. End with
READY-FOR-HUMAN or NOT-READY plus the smallest set of fixes needed.
