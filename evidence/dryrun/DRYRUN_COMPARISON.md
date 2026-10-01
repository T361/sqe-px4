# P14 dry-run — reproduction comparison

## What was done
1. Built `deliverables/24i3015_24i3166_24i3158_B.patch` from the submitted `sqe-a2` branch vs. `v1.17.0`.
2. Verified it applies cleanly to a **completely fresh** v1.17.0 worktree (`tools/sqe_make_patch.sh verify`) —
   log: `evidence/gates/G14/patch_verify.log`.
3. Created a second, independent worktree at `/tmp/sqe-dryrun` (not reused from the main build), applied the patch
   there, initialized all 29 submodules from scratch, and ran the complete Appendix A reproduction sequence:
   `make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__` → `ctest -R Sqe` → `tools/sqe_coverage.sh final`.
4. Build log: `evidence/dryrun/dryrun_build.log`. All 1696 targets built clean in 3m49s. `ctest -R Sqe`: 5/5
   binaries, 100% passed. Coverage capture: `evidence/dryrun/final/` (copied out before the main `evidence/coverage/`
   tree was restored from git — see note below).

## Result: numbers match exactly
| File | Submitted (`evidence/coverage/final/per_file.md`) | Dry-run (`evidence/dryrun/final/per_file.md`) |
|---|---|---|
| DataValidator.cpp | 100.0% line / 100.0% branch | 100.0% line / 100.0% branch |
| DataValidatorGroup.cpp | 99.4% line / 94.8% branch | 99.4% line / 94.8% branch |
| FailureDetector.cpp | 100.0% line / 74.4% branch | 100.0% line / 74.4% branch |
| FailureInjector.cpp | 100.0% line / 82.5% branch | 100.0% line / 82.5% branch |
| **Total** | 99.8% line / 82.3% branch | 99.8% line / 82.3% branch |

`diff evidence/coverage/final/per_file.md evidence/dryrun/final/per_file.md` → **no output (byte-identical)**.
No "documented timing-dependent SITL baseline effects" needed invoking — the numbers reproduced exactly on the
first dry-run attempt, with no retry needed.

## A process note, for transparency
`tools/sqe_coverage.sh`'s output directory defaults to the main repo's `evidence/coverage/` regardless of which
`PX4_DIR` it's pointed at. Running `PX4_DIR=/tmp/sqe-dryrun bash tools/sqe_coverage.sh final` therefore
**overwrote** the already-committed `evidence/coverage/final/` with the dry-run's own capture (same numbers,
different absolute file paths inside `scope.info` — a 252-line diff that was entirely path-string churn, not a
coverage difference). This was caught immediately by checking `git status`/`git diff --stat` right after running
the capture, before anything was committed. The dry-run's output was copied to its own location
(`evidence/dryrun/final/`) and the original `evidence/coverage/` tree was restored with `git checkout --` before
any of this was committed — confirmed clean (`git status --short evidence/coverage/` → 0 lines) prior to the commit
that includes this comparison file. If the dry-run pipeline is rerun by a human later, point `EVID` at a separate
directory explicitly (e.g. `EVID=/tmp/dryrun-coverage PX4_DIR=... bash tools/sqe_coverage.sh final`) to avoid this
trap rather than relying on catching it after the fact.

## Conclusion
The patch is self-contained, applies cleanly from a clean v1.17.0 checkout, and reproduces the exact coverage
numbers reported in `deliverables/report/REPORT.md` §6 on a completely independent build. Gate G14's "dry-run
numbers match (or differences explained)" checklist item is satisfied with an exact match, no differences to
explain.
