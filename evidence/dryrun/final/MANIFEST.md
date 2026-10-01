# Coverage capture manifest — final
UTC: 2026-10-01T16:33:44Z
Command: tools/sqe_coverage.sh final
CMAKE_BUILD_TYPE: Coverage
lcov: lcov: LCOV version 2.0-1 · genhtml: genhtml: LCOV version 2.0-1 · gcov tool: gcov (gcov (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0)
lcovrc: /etc/lcovrc · rc flags: --rc branch_coverage=1 · ignore/filter: --ignore-errors mismatch
capture dirs: --directory /tmp/sqe-dryrun/build/px4_sitl_test/src/modules/sensors/data_validator --directory /tmp/sqe-dryrun/build/px4_sitl_test/src/modules/commander/failure_detector
scope patterns: */sensors/data_validator/DataValidator.cpp */sensors/data_validator/DataValidatorGroup.cpp */commander/failure_detector/FailureDetector.cpp */commander/failure_detector/FailureInjector.cpp
ctest args: -E antlr4_tests_NOT_BUILT
ctest exit code: 0
tests run: 152
Notes: none
