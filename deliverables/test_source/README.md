# Student-authored test sources (SE3002 Assignment 02, group 24i3015_24i3166_24i3158_B)

Files created or modified by the group, at their paths inside PX4-Autopilot v1.17.0
(commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`). They are exactly the result of applying
`24i3015_24i3166_24i3158_B.patch` to that commit; the patch is the authoritative record. No production file is modified.

| File | Binary (ctest name) | Tests |
|---|---|---|
| `src/modules/sensors/data_validator/SqeDataValidatorTest.cpp` | unit-SqeDataValidator | 21 |
| `src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp` | unit-SqeDataValidatorGroup | 35 (22 MC/DC) |
| `src/modules/sensors/data_validator/SqeDataValidatorGroupAllocTest.cpp` | unit-SqeDataValidatorGroupAlloc | 3 (operator new fake) |
| `src/modules/commander/failure_detector/SqeFailureDetectorTest.cpp` | functional-SqeFailureDetector | 36 |
| `src/modules/commander/failure_detector/SqeFailureDetectorImuTest.cpp` | functional-SqeFailureDetectorImu | 8 |
| `src/modules/commander/failure_detector/SqeFailureDetectorCopyFaultTest.cpp` | functional-SqeFailureDetectorCopyFault | 1 (orb_data_copy fake, --wrap) |
| `src/modules/commander/failure_detector/SqeFailureInjectorTest.cpp` | functional-SqeFailureInjector | 14 + 2 disabled sanitizer probes |
| `src/modules/commander/failure_detector/SqeFailureInjectorParamFaultTest.cpp` | functional-SqeFailureInjectorParamFault | 2 (param_get stub, --wrap) |
| `src/lib/battery/SqeBatteryTest.cpp` | functional-SqeBattery | 31 (seam + orb_data_copy fake) |
| `src/modules/land_detector/SqeMcLandDetectorTest.cpp` | functional-SqeMcLandDetector | 46 (21 MC/DC, test subclass seam) |
| `src/modules/land_detector/SqeLandDetectorRunTest.cpp` | functional-SqeLandDetectorRun | 12 (real work queue) |
| `src/modules/land_detector/SqeLandDetectorBootTest.cpp` | functional-SqeLandDetectorBoot | 1 (first cycle, own process) |
| 4 x `CMakeLists.txt` (data_validator, failure_detector, battery, land_detector) | — | test registration; the two fault binaries add `target_link_options(... -Wl,--wrap=...)` |

Build and run (Ubuntu 24.04 or WSL2, after `Tools/setup/ubuntu.sh --no-nuttx --no-sim-tools`):

```
git apply 24i3015_24i3166_24i3158_B.patch            # or copy src/ over a clean v1.17.0 checkout
PX4_CMAKE_BUILD_TYPE=Coverage make tests TESTFILTER=Sqe
cd build/px4_sitl_test && ctest -R Sqe                # 12/12 binaries, 210 tests
```
