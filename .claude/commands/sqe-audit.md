---
description: Independent GenAI-style audit of an artifact (test file, matrix, report section)
argument-hint: <path to artifact>
---
Use the sqe-auditor agent on $ARGUMENTS. Apply: derive (from source) → compare with artifact → execute (re-run the relevant tests or
scripts) → check oracles, state control, boundaries, missing behaviour → decide ACCEPT / REVISE / REJECT with reasons.
Write the result to work/audit/ with a timestamped filename.
