# SPEC_10 — Investigation protocol, findings and gaps (binding)

## 1. Investigation protocol for any FAIL, BLOCKED or surprising PASS
Write `work/findings/INV-nn.md` with these steps **in order** (lecture 3: a failed test is not automatically a defect):
1. **Setup** — are params/messages/time/state what the test intended? Add temporary prints or asserts in the *test*, never in production.
2. **Expected result** — cite the oracle source. Is it justified by the test basis, or was it assumed?
3. **State/data** — hidden state from earlier calls/tests? Run the test alone and shuffled.
4. **Environment** — compiler, flags (`-freciprocal-math`, `-O0`), lcov/gcov version, OS timing.
5. **Dependency behaviour** — uORB latest-sample semantics, param defaults, Hysteresis/AlphaFilter contracts.
6. **Reproduce** — 3 consecutive runs, same result; record the command.
Only then classify: **product defect** · **wrong oracle** · **setup error** · **environment** · **automation defect** · **specification ambiguity**.

## 2. Finding record (`work/FINDINGS.md`)
```
### F-nn <short title>                               Status: candidate | confirmed | rejected | ambiguity
Location: <file:line>     Related tests: <IDs>     Evidence: <paths>
Reproduction: <minimal steps / command>
Expected: <behaviour + oracle source>            Actual: <observed>
Classification + rationale: <which class, why>
Severity (impact on the analysed behaviour) / reachability from production call sites: <…>
Report text: <2–4 sentences for §7>
```
## 3. Probes
Created only for **confirmed** findings with a justified oracle and a human approval (DECISIONS D-nnn). Named `DISABLED_PRBnn_*`, executed by
`tools/sqe_run_tests.sh probes`; the workbook row shows FAIL; the report explains it. Sanitizer probes (UB) run only in ASan/UBSan builds.

## 4. Gap record (`work/GAPS.md`)
```
### G-nn <file:line> <branch index / line>           Class: infeasible | environment-limited | tool-artefact | missing-test(open)
What exactly is uncovered: <statement / edge (e.g. "L213 branch 1 = best == nullptr")>
Why: <proof / investigation with evidence>
What would reach it: <environment, dependency, strategy (fault injection, NuttX target, HITL, param stub…)>
Counted in feasible coverage? yes/no
```
"Hardware dependent" alone is not accepted. Difficult logic is never excluded merely to raise the percentage.
