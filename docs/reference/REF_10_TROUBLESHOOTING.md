# REF_10 — Troubleshooting (symptom → cause → fix)
1. **Clone incomplete / submodule errors** → network interruption → `git submodule update --init --recursive`; re-check `git submodule status`.
2. **`ubuntu.sh` pip error (externally-managed-environment)** → PEP 668 → script uses `--break-system-packages` on Python ≥ 3.11; if run in a venv
   ensure the venv is active; never mix `sudo pip`.
3. **CMake configure fails downloading abseil/re2/googletest** → fuzztest FetchContent needs network once → retry; remove `build/px4_sitl_test`
   (the Makefile already deletes it on configure failure).
4. **Compiler killed (OOM) / WSL freeze** → `make tests -j2`; raise WSL memory/swap in `.wslconfig`; close browsers during the first build.
5. **`-Werror` in your test** → fix the warning (float compare, shadowing, unused, double promotion); no pragmas.
6. **Undefined reference to `hrt_absolute_time` / `px4_log_raw` in unit test** → link the module target (`modules__sensors`) instead of the bare
   library; fallback `data_validator px4_layer px4_platform`.
7. **Duplicate target name** → your file name collides after "Test" removal; use a unique `Sqe…` name.
8. **ctest does not list your test** → CMake not re-run after editing CMakeLists: `make tests TESTFILTER=__no_tests__ PX4_CMAKE_BUILD_TYPE=Coverage`.
9. **lcov "mismatch"/"negative"/"unused"/"inconsistent" error (lcov ≥ 2)** → known gcc/gcov quirks → `SQE_LCOV_EXTRA_IGNORE=negative,unused …` and
   note it in the MANIFEST. Never use `--rc geninfo_unexecuted_blocks=1` (it zeroes executed lines such as DataValidatorGroup L55 → false gaps).
10. **lcov 1.x "unknown argument for --ignore-errors: mismatch"** → you ran upstream `make tests_coverage` on lcov 1.15 → use `tools/sqe_coverage.sh`.
11. **All scope files 0 % / missing** → wrong build type (check `CMAKE_BUILD_TYPE` in CMakeCache), tests not run, or captured the wrong dir;
    run `tools/sqe_coverage.sh doctor`.
12. **"stamp mismatch" / "version mismatch" gcda warnings** → stale `.gcda` from an older build → captures always zero first; rebuild then recapture.
13. **macOS: `-O0-fprofile-arcs` error** → apply TOOLING-01 fix (P01 step 4), delete build dir; **macOS lcov needs llvm gcov** → `GCOV_TOOL=tools/llvm-gcov.sh`.
14. **Functional test passes alone, fails in suite** → leaked uORB message or param → publish-before-update, `param_reset_all()`, neutral vehicle_command
    in SetUp, separate binary for multi-instance topics.
15. **Flaky timing test** → increase waits (≥ 1.5×), never assert an upper time bound, run `--gtest_repeat=20`; if still flaky, record BLOCKED + INV.
16. **Death test warning about threads** → set death_test_style "threadsafe"; keep death tests in `*DeathTest` fixtures.
17. **CaptureStdout returns empty for PX4_INFO_RAW** → PX4 log went to a different FILE*; keep side-effect assertions, note limitation in DECISIONS.
18. **Build dir vanished** → configure failed and the Makefile removed it by design → fix, rebuild.
19. **Everything rebuilds after switching commands** → `PX4_CMAKE_BUILD_TYPE` or `PX4_ASAN/UBSAN` changed → stay in Coverage after P03; do sanitizer
    runs at the end and rebuild Coverage afterwards.
20. **Line numbers differ from REF docs** → not on v1.17.0 or a local edit → STOP (R1/R2); `git -C PX4-Autopilot diff v1.17.0 -- src ':!**/Sqe*'`.
21. **`param_find` returns PARAM_INVALID** → parameter not in this build config or typo; `param show FD_*` in SITL, or grep `failure_detector_params.c`.
23. **Branch columns empty / `BRF = 0` / script aborts "NO branch data"** → branch rc key not effective or the exception filter wiped branches
   (verified on lcov 2.0: `no_exception_branch=1` ⇒ 0 branches) → run `tools/sqe_coverage.sh doctor`; unset `SQE_LCOV_NO_EXCEPTION`; check the detected key.
22. **imbalanced-prop test sees another instance's data** → instance numbering depends on first publish order → create both PublicationMulti in
    `SetUpTestSuite` in fixed order, republish both in each test.
