# Student-authored test sources (SE3002 Assignment 02, group 24i3015_24i3166_24i3158_B)

These are the files created or modified by the group, laid out at their paths inside PX4-Autopilot v1.17.0
(commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`). They are exactly the result of applying
`24i3015_24i3166_24i3158_B.patch` to that commit; the patch is the authoritative record of the change.

| File | Status | Contents |
|---|---|---|
| `src/modules/sensors/data_validator/SqeDataValidatorTest.cpp` | new | 20 unit tests + 2 disabled probes (DataValidator) |
| `src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp` | new | 35 unit tests incl. 22 MC/DC tests + 1 disabled probe (DataValidatorGroup) |
| `src/modules/sensors/data_validator/SqeDataValidatorGroupAllocTest.cpp` | new | 3 allocation-failure unit tests (own binary) |
| `src/modules/sensors/data_validator/CMakeLists.txt` | modified | registers the 3 unit-test binaries above |
| `src/modules/commander/failure_detector/SqeFailureDetectorTest.cpp` | new | 36 functional tests (FailureDetector) |
| `src/modules/commander/failure_detector/SqeFailureDetectorImuTest.cpp` | new | 8 functional tests (imbalanced-prop / IMU path) |
| `src/modules/commander/failure_detector/SqeFailureInjectorTest.cpp` | new | 13 functional tests + 2 disabled sanitizer probes (FailureInjector) |
| `src/modules/commander/failure_detector/CMakeLists.txt` | modified | registers the 3 functional-test binaries above |

No production source file is modified. To use the files instead of the patch, copy `src/` over a clean v1.17.0
checkout, then build and run as in Appendix A of the report:

```
make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=Sqe
```
