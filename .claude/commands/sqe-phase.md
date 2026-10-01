---
description: Execute one SQE A2 phase (e.g. /sqe-phase P03) and stop at its gate
argument-hint: <phase id, e.g. P05>
---
Execute phase $ARGUMENTS:
1. Open the matching file in docs/plan/ (P00…P15). Check its Entry criteria against work/STATUS.md; stop if unmet.
2. Create a TodoWrite list from the phase Steps.
3. Execute the steps in order, saving every log/output under evidence/ as the phase specifies.
4. Fill the phase checklist in work/STATUS.md with evidence paths.
5. Write work/explain/$ARGUMENTS.md (plain-language explain-back + 3 likely viva questions with answers).
6. Invoke the sqe-auditor agent on the gate. Mark the gate READY-FOR-HUMAN and stop. Do not start the next phase.
