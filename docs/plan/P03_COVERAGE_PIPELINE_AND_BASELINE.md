# P03 — Coverage pipeline bring-up + baseline coverage (before student tests)
**Goal:** a trustworthy, reproducible gcov/lcov pipeline with branch data, and the authentic baseline for the scope. **Owner:** COV.
**Gate:** G03. **Spec:** `docs/specs/SPEC_03_COVERAGE_PIPELINE.md` (binding).

## Why not `make tests_coverage`
It runs `make clean` (full rebuild each time), captures without branch coverage, emits only `coverage/lcov.info` (no HTML), and passes
`--ignore-errors mismatch` which lcov 1.x rejects. Our script keeps the same compiler flags (`PX4_CMAKE_BUILD_TYPE=Coverage` →
`--coverage -fprofile-update=atomic`, GCC: `-O0 -fno-default-inline -fno-inline -fno-elide-constructors`) and adds branch data,
zero-coverage (initial) records, scope extraction, HTML and a manifest.

## Steps
1. `tools/sqe_coverage.sh doctor` — prints lcov/genhtml/gcov versions, detected rc keys (branch, exception), gcov tool, build type in cache.
2. `tools/sqe_coverage.sh build` — `make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__` (reconfigures the same build dir;
   full rebuild). Log → `evidence/build/coverage_build.log`. Verify `grep CMAKE_BUILD_TYPE PX4-Autopilot/build/px4_sitl_test/CMakeCache.txt` = Coverage.
3. `tools/sqe_coverage.sh baseline` — zero counters → `ctest -E 'Sqe'` (all upstream tests) → capture initial + run → merge → extract the
   4 scope files → HTML → summaries → MANIFEST. Output: `evidence/coverage/baseline/`.
4. Repeat step 3 once more into `evidence/coverage/baseline_run2/` (`tools/sqe_coverage.sh baseline baseline_run2`) to check determinism
   (the 10 s `sitl-imu_filtering` test is timing-dependent). If numbers differ, report the range (RK-08).
5. `python3 tools/sqe_lcov_report.py summary evidence/coverage/baseline/scope.info` → confirm **BRF > 0** (branch data really present).
   Expected pattern (verify!): DataValidator*.cpp partially covered via `sitl-imu_filtering` (nominal single-IMU path, `sensors status` print);
   FailureDetector.cpp / FailureInjector.cpp 0 % (no upstream test starts commander) — they must still appear thanks to the initial capture.
6. Write `work/coverage_iterations.md` entry `IT-0 baseline` with per-file line/branch/function numbers (copied from per_file.md).

## After this phase
Stay in the Coverage build type for all development (tests run fine, just slower). Switching back costs a full rebuild.

## Gate G03 checklist
- [ ] MANIFEST records git HEAD, tool versions, rc flags, ctest regex and exit code
- [ ] scope.info contains all 4 files (grep `SF:`), BRF > 0
- [ ] HTML opens (`evidence/coverage/baseline/html/index.html`) and shows branch columns
## Explain-back
Line vs branch vs function counts in lcov; why gcov has more "branches" than decision outcomes (one pair per short-circuit operand,
plus compiler-generated edges); why initial capture is needed for 0 % files; why baseline must precede student tests.
