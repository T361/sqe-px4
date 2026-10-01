# P02 — Baseline build + upstream test execution
**Goal:** prove PX4 and its existing test infrastructure run locally **before** any student change. **Owner:** ENV. **Gate:** G02.

## Facts (verified in v1.17.0 Makefile / CMake)
- `make tests` = CMake config `px4_sitl_test` (board `boards/px4/sitl/test.px4board`: default SITL + `CONFIG_BOARD_NOLOCKSTEP=y`),
  target `test_results` → `ctest --output-on-failure -T Test [-R $TESTFILTER] --exclude-regex antlr4_tests_NOT_BUILT` in `build/px4_sitl_test`.
- ctest contains: ~119 GTest registrations (`unit-*`, `functional-*`), SITL script tests (`sitl-*`, e.g. `sitl-imu_filtering` which runs
  `fake_imu` + `sensors` for 10 s), `posix_*` command tests and `dyn`. GoogleTest comes from the `test/fuzztest` submodule (FetchContent
  may download abseil/re2 on first configure → needs network once).
- `make tests -j2` limits Ninja parallelism (use on ≤ 8 GB RAM / WSL).

## Steps
1. `cd PX4-Autopilot && (time make tests) 2>&1 | tee ../evidence/baseline/make_tests.log` (first run 20–90 min).
2. `cd build/px4_sitl_test && ctest -N | tee ../../../evidence/baseline/ctest_list.txt` and
   `cp Testing/Temporary/LastTest.log ../../../evidence/baseline/LastTest_baseline.log`.
3. Extract the summary: `grep -E "tests passed|tests failed|The following tests FAILED" -A20 ../evidence/baseline/make_tests.log > ../evidence/baseline/summary.txt`.
4. If any upstream test failed: re-run it alone (`ctest -R <name> --output-on-failure`), record in `work/DECISIONS.md` as
   `PRE-EXISTING: <name>, reason, log path`. Do **not** fix or claim it; mention it in the report's environment section.
5. Optional (assignment suggests it): `make px4_sitl` → `evidence/baseline/make_px4_sitl.log` (builds default SITL; not needed for tests).
6. Check that the orphaned data_validator tests are indeed not registered: `grep -c ecl_tests_data_validator evidence/baseline/ctest_list.txt` → 0.
   (They link a non-existent `ecl_validation` target and `data_validator/CMakeLists.txt` never adds `tests/`.) Record as observation O-01.

## Gate G02 checklist
- [ ] make_tests.log ends with a ctest summary line; total test count recorded in STATUS
- [ ] pre-existing failures (if any) documented, not modified
- [ ] O-01 recorded
## Explain-back
What `make tests` actually runs (unit vs functional vs SITL tests), how TESTFILTER works (ctest `-R`), why the first build is slow.
