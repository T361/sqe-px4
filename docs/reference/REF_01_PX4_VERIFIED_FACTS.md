# REF_01 — Facts verified against the PX4-Autopilot v1.17.0 source
Status legend: **V** = read in the v1.17.0 source while preparing the kit · **E** = expected behaviour of tools on your machine (verify in P01–P03).

## Baseline
| Fact | Status | Source |
|---|---|---|
| `refs/tags/v1.17.0` = tag object `a5eb12d2ab591251faa009f76b2685b8cc64405d`, peeled commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4` | V | `git ls-remote --tags` |
| HEAD commit date 2026-01-16 05:40:09 +0900, subject "Update NuttX fmu-v6x config use with Zenoh (#26213)" | V | `git log -1` |
| No `CLAUDE.md`, `AGENTS.md` or `.claude/` in the PX4 root; PX4 has its own `docs/` site folder → keep the kit outside the clone | V | `ls` |

## Build and test infrastructure
| Fact | Status | Source |
|---|---|---|
| `make tests`: `-DTESTFILTER=$(TESTFILTER)`, target `test_results`, config `px4_sitl_test`, ASAN/UBSAN colour options | V | Makefile L406–411 |
| `test_results`: `ctest --output-on-failure -T Test [-R filter] --exclude-regex "antlr4_tests_NOT_BUILT"`, depends on `px4`, `examples__dyn_hello` | V | test/CMakeLists.txt |
| `make tests_coverage`: `make clean` → `tests PX4_CMAKE_BUILD_TYPE=Coverage` → `lcov --directory build/px4_sitl_test --base-directory … --gcov-tool gcov --capture --ignore-errors mismatch -o coverage/lcov.info` (no branch rc, no HTML) | V | Makefile L413–424 |
| `PX4_CMAKE_BUILD_TYPE` → `-DCMAKE_BUILD_TYPE`; else `PX4_ASAN`/`PX4_MSAN`/`PX4_TSAN`/`PX4_UBSAN` choose sanitizer build types | V | Makefile L139–160 |
| cmake-cache-check reconfigures when cached `-D` options differ; on configure failure the build dir is removed (`|| (rm -rf $(BUILD_DIR))`) | V | Makefile L173–204 |
| `make … -jN` → Ninja `-jN`; otherwise Ninja default parallelism | V | Makefile L77–120 |
| Coverage flags: all targets `--coverage -fprofile-update=atomic`; GCC CXX `--coverage -ftest-coverage -fprofile-arcs -O0 -fno-default-inline -fno-inline -fno-elide-constructors`; Clang CXX string contains `-O0-fprofile-arcs` (missing space) | V | cmake/coverage.cmake |
| `px4_add_unit_gtest(SRC … [EXTRA_SRCS] [COMPILE_FLAGS] [INCLUDES] [LINKLIBS])` → executable `unit-<name without "Test">`, `link_fuzztest()`, `add_test(… WORKING_DIRECTORY ${PX4_BINARY_DIR})` | V | cmake/px4_add_gtest.cmake |
| `px4_add_functional_gtest` → `functional-<name>`, links LINKLIBS + gtest_functional_main px4_layer px4_platform uORB systemlib cdev px4_work_queue px4_daemon work_queue parameters events perf tinybson uorb_msgs fuzztest::fuzztest test_stubs; `MODULE_NAME` defined | V | cmake/px4_add_gtest.cmake |
| `gtest_functional_main.cpp`: `InitGoogleTest`, fuzztest init, `uORB::Manager::initialize(); param_init();` | V | platforms/posix/src/px4/common/gtest_runner/ |
| GoogleTest is provided through the `test/fuzztest` submodule (`add_subdirectory(fuzztest)`) | V | test/CMakeLists.txt |
| `BUILD_TESTING` from `include(CTest)`; `px4_sitl_test` board = `CONFIG_BOARD_NOLOCKSTEP=y` (+ lightware driver) | V | CMakeLists.txt L348; boards/px4/sitl/test.px4board |
| Global warnings: `-Wall -Wextra -Werror -Warray-bounds -Wcast-align -Wdisabled-optimization -Wdouble-promotion -Wfatal-errors -Wfloat-equal -Wformat-security -Winit-self -Wlogical-op -Wpointer-arith -Wshadow -Wuninitialized -Wunknown-pragmas -Wunused-variable`; GCC `-fcheck-new`; math `-fno-signed-zeros -fno-trapping-math -freciprocal-math -fno-math-errno` (no fast/finite-math) | V | cmake/px4_add_common_flags.cmake |
| RTTI enabled for SITL/BUILD_TESTING; no `-fno-exceptions` on SITL | V | same |
| `PX4_ISFINITE(x)` = `__builtin_isfinite(x)` | V | platforms/common/include/px4_platform_common/defines.h:58 |
| SITL ctest list: `sitl-{atomic_bitset, bitset, bson, dataman, file2, float, hrt, int, IntrusiveQueue, IntrusiveSortedList, List, mathlib, matrix, param, parameters, perf, search_min, sleep, versioning}`, `sitl-{controllib_test, lightware_laser_test, rc_tests, uorb_tests}`, `sitl-imu_filtering`, `dyn`, `posix_{hrt_test,cdev_test,wqueue_test}` | V | platforms/posix/cmake/sitl_tests.cmake |
| `test_imu_filtering` script: `fake_imu start`, `sensors start`, `gyro_fft start`, 10 s, `sensors status` → exercises DataValidatorGroup nominal path + print | V | posix-configs/SITL/init/test/test_imu_filtering |
| CI container `px4io/px4-dev:v1.16.0-rc1-258-g0369abd556`; CI runs `make tests_coverage` and uploads `coverage/lcov.info` | V | .github/workflows/checks.yml |
| `Tools/setup/ubuntu.sh` supports 24.04 & 22.04; flags `--no-nuttx`, `--no-sim-tools`; installs cmake, g++, gcc, lcov, ninja-build, python3-*; pip `--break-system-packages` when Python ≥ 3.11 | V | Tools/setup/ubuntu.sh |
| `Tools/setup/macos.sh`: Homebrew `px4-dev`, pip requirements, `--sim-tools` optional | V | Tools/setup/macos.sh |
| 119 `px4_add_*_gtest` registrations under `src/`; none for data_validator or failure_detector | V | grep |
| `data_validator/tests/` is orphaned: links `ecl_validation` (non-existent), never added via `add_subdirectory` | V | data_validator/{CMakeLists.txt,tests/CMakeLists.txt} |
| `data_validator` and `failure_detector` are plain `px4_add_library` (link only `prebuild_targets`); modules link `prebuild_targets px4_platform systemlib perf … px4_layer uORB` | V | cmake/px4_add_library.cmake, px4_add_module.cmake |
| `modules__sensors` DEPENDS data_validator; `modules__commander` DEPENDS failure_detector; upstream unit test links a module (`ManualControlSelectorTest` → `modules__manual_control`; `mag_calibration_test` → `modules__commander`) | V | CMakeLists of sensors/manual_control/commander |

## Runtime semantics used by the tests
| Fact | Status | Source |
|---|---|---|
| POSIX `hrt_absolute_time()` (no lockstep) = `CLOCK_MONOTONIC` via `ts_to_abstime` (time since boot, not since process start) | V | platforms/posix/src/px4/common/drv_hrt.cpp |
| New uORB subscriber: `initial_generation = generation − (data_valid ? 1 : 0)` → sees the last published sample as updated | V | platforms/common/uORB/uORBDeviceNode.cpp:454–464 |
| `Subscription::ChangeInstance(i)`: true if already on i, or if node i exists (then re-subscribes); false otherwise | V | platforms/common/uORB/Subscription.cpp:76–93 |
| `ORB_MULTI_MAX_INSTANCES` = 10 (4 only with `CONSTRAINED_MEMORY`) | V | platforms/common/uORB/uORB.h |
| `ModuleParams::updateParams()` is **protected**; `Param<T>` constructors call `update()` (value read at construction) | V | px4_platform_common/module_params.h, param.h |
| `param_reset_all()` resets without notification, autosaves only if autosave enabled; `param_control_autosave(bool)` exists | V | src/lib/parameters/{param.h,parameters.cpp} |
| `px4_log_raw()` writes to `stdout` (POSIX: `get_stdout()`), INFO level and above | V | platforms/common/px4_log.cpp |
| Upstream functional-test idiom: fixture `: public ::testing::Test, ModuleParams` with `ModuleParams(nullptr)`, `param_control_autosave(false)`, `_param.set(); _param.commit();`, test subclass exposing protected methods | V | src/modules/rc_update/RCUpdateTest.cpp |
| Hysteresis: state flips when `now ≥ last_change_request + time_from_<state>`; default times 0 | V | src/lib/hysteresis/hysteresis.cpp |

## Message constants (msg/ and msg/versioned/)
VehicleStatus: `arming_state`, ARMING_STATE_DISARMED=1, ARMING_STATE_ARMED=2, `vehicle_type` ROTARY_WING=1 FIXED_WING=2, `is_vtol_tailsitter`,
`in_transition_mode` · VehicleControlMode: `flag_control_attitude_enabled` · EscStatus: CONNECTED_ESC_MAX=8, `esc_count`, `esc_online_flags`,
`esc_armed_flags`, `esc[8]` · EscReport: `timestamp`, `esc_rpm` (int32), `esc_voltage`, `esc_current`, `actuator_function` (uint8), `failures` (uint16) ·
ActuatorMotors: ACTUATOR_FUNCTION_MOTOR1=101, NUM_CONTROLS=12, `control[12]` · VehicleCommand: VEHICLE_CMD_DO_SET_MODE=176,
VEHICLE_CMD_INJECT_FAILURE=420, FAILURE_UNIT_SYSTEM_MOTOR=101, FAILURE_TYPE_OK 0/OFF 1/STUCK 2/GARBAGE 3/WRONG 4/SLOW 5/DELAYED 6/INTERMITTENT 7,
ORB_QUEUE_LENGTH=8, `param1..3`, `command`, `from_external` · VehicleCommandAck: RESULT_ACCEPTED=0, RESULT_UNSUPPORTED=3, ORB_QUEUE_LENGTH=4 ·
PwmInput: `pulse_width` (µs) · SensorSelection: `accel_device_id` · VehicleImuStatus: `accel_device_id`, `var_accel[3]`, `timestamp` · VehicleAttitude: `q[4]`.

## Production call sites (safety relevance)
`voted_sensors_update.h:114` `DataValidatorGroup voter{1};` · `voted_sensors_update.cpp:195–196` accel/gyro `get_best` · `:291–345` `checkFailover()`
uses `failover_count()`, `failover_state()`, `failover_index()`, lowers the failed sensor's priority · `:368` `add_new_validator()` for each further
sensor · `:419/:426` `accel_healthy/gyro_healthy = (get_sensor_state(i) == NO_ERROR)` · `VehicleMagnetometer.hpp:157` `_voter{1}`,
`.cpp:56` threshold 1000, `:505` `add_new_validator`, `:536` `get_best`, `:640–685` failover reporting · `sensors.cpp:70–71` airspeed validator
timeout 300000 µs, threshold 100, `:285` put `[dp, 0, 0]` priority 100, `:338` `confidence(hrt_absolute_time())`, `:693` `print()` ·
FailureDetector is owned and updated by Commander (`src/modules/commander/Commander.cpp`); its flags feed the failsafe/health logic.

## Tool facts (external, verify locally)
| Fact | Status |
|---|---|
| Ubuntu 24.04 lcov 2.0-1: `/etc/lcovrc` keys `branch_coverage` (L280) and `no_exception_branch` (L246); `--rc branch_coverage=1`, `--branch-coverage` and the old `lcov_branch_coverage=1` all produced branch data; adding `no_exception_branch=1` produced **none** | **V (tested in kit sandbox)** |
| lcov ≥ 2.3 adds `mcdc_coverage` | E (man pages) |
| lcov 1.x: `--rc lcov_branch_coverage=1`, exception filter `--rc geninfo_no_exception_branch=1`; `--ignore-errors` accepts gcov/source/graph only | E |
| GCC ≥ 14: `-fcondition-coverage` + `gcov --conditions` = masking MC/DC; Clang ≥ 18: `-fcoverage-mcdc` | E |
