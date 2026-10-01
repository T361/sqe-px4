# P10 — Coverage measurement + iteration to the maximum practically achievable
**Goal:** final evidence of statement/line and branch coverage for the four scope files, and a record showing you did not stop early.
**Owner:** COV (+IMPL). **Gate:** G10. **Spec:** SPEC_03. **Rubric:** Part 3B (20 marks).

## Loop (repeat until only justified gaps remain)
1. `tools/sqe_run_tests.sh build && tools/sqe_run_tests.sh run` (all Sqe binaries green, XML in `evidence/tests/xml/`).
2. `tools/sqe_coverage.sh student IT-n` — zero counters → `ctest -R Sqe` → capture → `evidence/coverage/IT-n/`.
3. `python3 tools/sqe_lcov_report.py gaps evidence/coverage/IT-n/scope.info > work/gaps_IT-n.md` — lists every line with 0 hits and every
   branch with `-`/`0` taken, grouped by function, with the source text.
4. For each item decide: **missing-test** (write a catalogue row + test, rerun) · **infeasible** (write the proof) · **environment-limited**
   (state the environment/dependency/strategy that would reach it — "hardware dependent" alone is not accepted) · **tool-artifact**
   (compiler-generated edge: `-fcheck-new` null check after `new`, exception edge on a call, destructor/cleanup edge — show the gcov
   branch and why no source-level outcome corresponds).
5. Append `IT-n` to `work/coverage_iterations.md`: date, tests added, per-file line/branch numbers, items resolved, items reclassified.

## Final captures (after the last iteration)
```bash
tools/sqe_coverage.sh final          # all tests (upstream + Sqe) → evidence/coverage/final/
tools/sqe_coverage.sh student        # Sqe only → evidence/coverage/student/ (shows the suite's own contribution)
tools/sqe_coverage.sh baseline upstream_final   # upstream only on the final tree (should equal P03 baseline; explains contribution)
python3 tools/sqe_lcov_report.py compare evidence/coverage/baseline/scope.info evidence/coverage/final/scope.info > evidence/coverage/compare_baseline_final.md
```
Optional tool-supported MC/DC (GCC ≥ 14): SPEC_04 §8 — separate build directory, `gcov --conditions` for DataValidatorGroup.cpp.

## Reporting rules (feed report §6)
- Report raw tool numbers from `per_file.md` and, separately, "feasible" numbers that exclude items proven infeasible (lecture 6 reporting rule).
- Explain gcov's branch semantics once (condition-level edges) so the examiner reads 100 % branch correctly.
- Never add exclusion markers (`LCOV_EXCL_*`) to production code (R2). Exclusions are applied only in the analysis, with proofs.

## Gate G10 checklist
- [ ] final + student + upstream_final captures with MANIFESTs
- [ ] every uncovered item appears in `work/GAPS.md` (ID, file:line, gcov branch index, class, justification, strategy)
- [ ] coverage_iterations.md shows ≥ 2 iterations after the first full run
## Explain-back
Predict what coverage drops if SQE-DVG-MC13 or SQE-FD23 were removed (use `tools/sqe_unique_coverage.py` output), explain one infeasible gap proof.
