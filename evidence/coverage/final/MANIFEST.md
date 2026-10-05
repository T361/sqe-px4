# Coverage capture manifest — final
UTC: 2026-10-05T16:17:35Z
Command: tools/sqe_coverage.sh final
PX4 HEAD: bba5a3d6e7f086201088c8db5f98c28d3d3b0630 (branch sqe-a2)
Dirty files: 2
CMAKE_BUILD_TYPE: Coverage
lcov: lcov: LCOV version 2.0-1 · genhtml: genhtml: LCOV version 2.0-1 · gcov tool: gcov (gcov (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0)
lcovrc: /etc/lcovrc · rc flags: --rc branch_coverage=1 · ignore/filter: --ignore-errors mismatch
capture dirs: --directory /home/dns/Desktop/sqe-a2-px4-kit/PX4-Autopilot/build/px4_sitl_test/src/modules/sensors/data_validator --directory /home/dns/Desktop/sqe-a2-px4-kit/PX4-Autopilot/build/px4_sitl_test/src/modules/commander/failure_detector
scope patterns: */sensors/data_validator/DataValidator.cpp */sensors/data_validator/DataValidatorGroup.cpp */commander/failure_detector/FailureDetector.cpp */commander/failure_detector/FailureInjector.cpp
ctest args: -E antlr4_tests_NOT_BUILT
ctest exit code: 0
tests run: 153
Notes: none
