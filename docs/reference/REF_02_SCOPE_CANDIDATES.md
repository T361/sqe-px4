# REF_02 — Scope candidate matrix (pre-screened at v1.17.0; re-verify in P04)
Metrics from `wc -l` / grep heuristics (decisions ≈ `if|while|for|?|case`, compound ≈ lines with `&&`/`||`).

## 1. Candidates
| Candidate | Lines | Decisions~ | Compound~ | Upstream tests? | Dependencies | Verdict |
|---|---|---|---|---|---|---|
| sensors/data_validator/DataValidator.cpp | 155 | 17 | 0 | none (legacy tests orphaned) | none at runtime (hrt/log only in print) | **Selected (A)** |
| sensors/data_validator/DataValidatorGroup.cpp | 347 | 42 | 7 | none | DataValidator | **Selected (B, MC/DC)** |
| commander/failure_detector/FailureDetector.cpp | 354 | 46 | 15 | none | params, 6 uORB topics, time, Hysteresis, AlphaFilter | **Selected (C)** |
| commander/failure_detector/FailureInjector.cpp | 134 | 17 | 4 | none | SYS_FAILURE_EN, vehicle_command(+ack) | **Selected (D)** |
| lib/battery/battery.cpp | 434 | 40 | 15 | none | params, uORB pub, time, filters | stretch candidate (not needed) |
| land_detector/MulticopterLandDetector.cpp + LandDetector.cpp | 324+259 | 63 | 34 | none | ModuleBase/WorkItem, many topics, protected API | rejected: heavy harness, time-driven state machine; better SITL |
| commander/HealthAndArmingChecks/checks/batteryCheck.cpp | 324 | 49 | 19 | parent has HealthAndArmingChecksTest | Context/Report framework | rejected: framework coupling |
| commander/failsafe/{failsafe,framework}.cpp | 718+740 | 210 | 97 | failsafe_test.cpp | large state machine | rejected: size + existing tests |
| manual_control/ManualControlSelector.cpp | 135 | 16 | 15 | ManualControlSelectorTest | none | rejected: already tested (small contribution) |
| navigator/GeofenceBreachAvoidance | 293 | 15 | 2 | yes | geo | rejected: few compound decisions, tested |
| commander/Safety.cpp, UserModeIntention.cpp | 78, 102 | 5, 10 | 1, 4 | yes (dir) | | rejected: trivial |
| lib/hysteresis/hysteresis.cpp | 92 | 8 | 2 | HysteresisTest | | rejected: tested; used as dependency |

## 2. Why A–D together form a substantial, coherent scope
~990 source lines, ~122 decisions, ~26 compound decisions across two safety chains: **sensor redundancy** (which IMU/mag feeds the estimator;
when a failover is declared and reported) and **failure detection** (attitude envelope, ESC arming, motor telemetry/under-current, imbalanced
props, external ATS) feeding Commander's failsafe. Both are pure control logic without GUI/presentation code. Baseline: A/B partially executed
only incidentally by `sitl-imu_filtering`; C/D not executed at all → the student suite's contribution is measurable.

## 3. Test-level justification (short form for the report)
A/B unit: explicit inputs, no params/uORB/time services → `px4_add_unit_gtest` (linked via `modules__sensors` only to resolve platform symbols of
`print()`) · C/D functional: params read at construction, uORB inputs incl. multi-instance topics, real time for hysteresis/timeouts →
`px4_add_functional_gtest` · SITL not needed: no drivers, work queues, module lifecycle or simulator dynamics are required to reach any decision.
