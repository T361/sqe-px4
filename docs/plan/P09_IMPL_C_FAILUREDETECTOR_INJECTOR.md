# P09 — Implementation chunk C: `FailureDetector` + `FailureInjector` (GTest functional)
**Goal:** statement + branch coverage of both files with deterministic control of params, uORB inputs and time. **Owner:** IMPL. **Gate:** G09.
**Inventory:** REF_05 (FD-D01…D45, FI-D01…D13).

## Files and registration (append to `src/modules/commander/failure_detector/CMakeLists.txt`)
```cmake
# SQE A2 (student-authored tests)
px4_add_functional_gtest(SRC SqeFailureDetectorTest.cpp LINKLIBS modules__commander)
px4_add_functional_gtest(SRC SqeFailureDetectorImuTest.cpp LINKLIBS modules__commander)
px4_add_functional_gtest(SRC SqeFailureInjectorTest.cpp LINKLIBS modules__commander)
```
→ `functional-SqeFailureDetector`, `functional-SqeFailureDetectorImu`, `functional-SqeFailureInjector`. (`ModeManagementTest` uses the same LINKLIBS.)
If link fails, fall back to `LINKLIBS failure_detector hysteresis` and log it. The IMU (imbalanced-prop) tests live in their own binary because
multi-instance `vehicle_imu_status` topics persist for the whole process.

## Fixture rules (SPEC_02 §6)
`SetUp()`: `param_control_autosave(false); param_reset_all();` · `ASSERT_GT(hrt_absolute_time(), 1_s)` · set params with
`param_set(param_find("FD_…"), &v)` **then** construct `FailureDetector fd{nullptr}` (or `std::make_unique`) · publish all inputs before the
first `update()` · pass `vehicle_status_s`/`vehicle_control_mode_s` structs directly · time waits with `std::this_thread::sleep_for` at ≥ 1.5×
the trigger time; "not yet" checks are made in the same `update()` that starts the hysteresis (deterministic) · never assert elapsed-time upper bounds.
Isolation defaults unless the test says otherwise: FD_FAIL_R/P_TTRI 0.02 s (documented minimum), FD_ACT_EN 0 in ESC-arming tests,
FD_ESCS_EN 0 in motor tests, FD_IMB_PROP_THR 0 except in IMU tests, SYS_FAILURE_EN 0 except FD31/FI tests.
Message constants (verified): ARMING_STATE_ARMED 2 / DISARMED 1; VEHICLE_TYPE_ROTARY_WING 1 / FIXED_WING 2; CONNECTED_ESC_MAX 8;
ACTUATOR_FUNCTION_MOTOR1 101; actuator_motors NUM_CONTROLS 12; VEHICLE_CMD_INJECT_FAILURE 420; FAILURE_UNIT_SYSTEM_MOTOR 101;
FAILURE_TYPE_OK 0/OFF 1/STUCK 2/GARBAGE 3/WRONG 4; VEHICLE_CMD_RESULT_ACCEPTED 0/UNSUPPORTED 3; ORB_MULTI_MAX_INSTANCES 10 (SITL).

## Catalogue — `SqeFailureDetectorTest` (IDs `SQE-FD-nn`, gtest `FDnn_…`)
| ID | Scenario | Expected | Decisions |
|---|---|---|---|
| FD01 | roll 70° with attitude control on → flag set; then `flag_control_attitude_enabled=false` | roll true, then roll/pitch/alt/ext false and `update()` returns true | D01T/F |
| FD02 | FD_FAIL_R 60, TTRI 0.02; roll 70°: update; wait 40 ms; publish again; update | first roll=false (hysteresis started), then roll=true, return true | D08T D12 T∧T |
| FD03 | roll 50° | roll false after wait | D12 T∧F |
| FD04 | FD_FAIL_R 0; roll 170° | roll false (check disabled) | D12 F (2nd NE) |
| FD05–07 | pitch analogues of FD02–FD04 | pitch true / false / false | D13 |
| FD08 | second `update()` without new attitude | flags unchanged, return false | D08F |
| FD09 | tailsitter, in_transition, roll 80° | no roll flag | D09T D10T |
| FD10 | tailsitter FW, attitude pitch −80° vs same attitude as ROTARY_WING | FW: no pitch flag (rotated pitch +10°); MC: pitch flag | D11T/F |
| FD11 | FD_EXT_ATS_EN 1, TRIG 1900, pulse 2000; wait 150 ms; publish again | ext true | D02T D14T D15 T∧T |
| FD12 | pulses 1899 / 1900 / 2999 / 3000 (each own detector) | F / T / T / F (window [TRIG, 3000 µs)) | D15 boundaries |
| FD13 | FD_EXT_ATS_EN 0, pulse 2000 | ext false | D02F |
| FD14 | ATS enabled, no new pwm_input | ext unchanged | D14F |
| FD15 | armed, 4 ESCs, armed flags 0x0F, failures 0; wait 350 ms | arm_escs false | D03T D04T D16T D17 D18F D19F |
| FD16 | flags 0x07 → update, wait 350 ms, publish, update | arm_escs true (300 ms hysteresis) | D19T |
| FD17 | flags 0x0F but esc[2].failures=1 | arm_escs true after 350 ms | D18 (2nd operand T) |
| FD18 | after FD16 trigger, healthy ESCs while armed | arm_escs stays true (latched while armed) | D19F with flag kept |
| FD19 | disarmed | arm_escs false | D16F |
| FD20 | esc_count 12, flags 0xFF | no failure (count clamped to 8) | clamp |
| FD21 | FD_ESCS_EN 0, flags 0x00 | no failure | D04F |
| FD22 | no esc_status published since last update | ESC/motor logic skipped | D03F |
| FD23 | FD_ACT_EN 1: ESC0 current 5 A fresh; then esc[0].timestamp = now−400 ms; again old; then fresh | motor true + `getMotorFailures()`&1; stays (no re-set); then motor false | D32 D33T/F D34T D42 D43T D44T |
| FD24 | ESC never reported current (0 A), old timestamp | no timeout flag (never valid) | D32F D33F D35F D36F |
| FD25 | THR 0.2, C2T 2.0, TOUT 10 ms: current 5 A; then control 0.5 & 0.1 A; wait 30 ms; publish again | motor true (under-current latched) | D36T D37T D38T D39T D41T |
| FD26 | timer started, then control 0.1 before timeout; wait; low current again | no under-current flag (timer reset) | D38F D40T |
| FD27 | control[0]=NaN with low current | no timer (throttle treated as 0) | D37F |
| FD28 | esc[0].actuator_function=0 (non-motor), old timestamp | ignored | D31T |
| FD29 | after FD25 under-current latch, disarm (publish esc_status to trigger the update) | motor false; `getMotorFailures()`==0 (under-current mask cleared) | D29F D45 |
| FD30 | FD_ACT_EN 0 | motor logic skipped | D05F |
| FD31 | SYS_FAILURE_EN 1: valid ESC → inject STUCK motor 1 → next esc update | FailureDetector sees blocked telemetry → motor timeout flag (injector in FD control path) | integration of FI-D10/D12 with FD-D33 |
| FD32 | return value: no change → false; change → true | per scenario | FD update return |
| FD33 | after FD23 steps 1–2 (ESC0 timed out, motor true), disarm + publish esc_status | **characterization F-15:** motor false but `getMotorFailures()` still 0x01 — the timed-out mask is not cleared on disarm | D29F |

## Catalogue — `SqeFailureDetectorImuTest` (IDs `SQE-FDI-nn`)
`SetUpTestSuite`: create two static `uORB::PublicationMulti<vehicle_imu_status_s>` and publish once in order (→ instances 0 and 1).
| ID | Scenario | Expected | Decisions |
|---|---|---|---|
| FDI01 | THR 30; selection id 1111; inst0 id 1111, var {40000,40000,0}, timestamp 10 s | imbalanced_prop true; metric ≈ 200/6 ≈ 33.3 (AlphaFilter α = dt/(τ+dt), dt clamped to 1 s, τ 5 s) | D06T D20T D21T D22F D26T D27T D28 T∧T |
| FDI02 | var {100,100,100} | false (metric 0) | D28 T∧T, threshold F |
| FDI03 | THR 0 | not evaluated; metric 0 | D06F |
| FDI04 | selection 2222; inst0 1111; inst1 2222 (big var) | detected through instance 1 | D22T D23 D24F D25 F/T |
| FDI05 | selection 3333 (no instance) | loop scans all 10, ChangeInstance false for 2..9, no update | D24T D28 (2nd F) |
| FDI06 | selection id 0 and inst0 id 0 | no computation (device id 0) | D22F D28 (1st F) |
| FDI07 | second update without new IMU/selection data | metric unchanged | D20F D26F |

## Catalogue — `SqeFailureInjectorTest` (IDs `SQE-FI-nn`), direct `FailureInjector` tests
`SetUp()`: params reset; publish a neutral `vehicle_command` (DO_SET_MODE 176) so a new subscriber never sees a previous test's injection;
ack subscription created and drained before acting.
| ID | Scenario | Expected | Decisions |
|---|---|---|---|
| FI01 | SYS_FAILURE_EN 0; inject OFF motor 1 | stop mask 0; no ack | D01 (2nd F), D02T |
| FI02 | enabled; OFF instance 1 | stop mask 0x01; ack ACCEPTED, command 420 | D01T D03 D04F D05 D06 D07(OFF) D08T |
| FI03 | OFF instance 0 | stop mask 0xFF | D06 (1st F) |
| FI04 | OFF(0) then OK(0); STUCK(1) then OK(1) | masks cleared; manipulate no longer changes ESCs | D07(OK) |
| FI05 | STUCK motor 2 on 4-ESC status | esc[1] zeroed except actuator_function 102; online flags 0x0D; others unchanged | D07(STUCK) D10T D11 D12T |
| FI06 | WRONG motor 1 | esc[0] voltage×0.1, current×0.1, rpm×10 | D07(WRONG) D13T |
| FI07 | type GARBAGE (3) | ack UNSUPPORTED; masks unchanged | D07(no case) D08F |
| FI08 | command DO_SET_MODE | ignored, no ack | D04 (1st T) |
| FI09 | INJECT with unit 0 | ignored, no ack | D04 (2nd T) |
| FI10 | OFF instance 9 | ack UNSUPPORTED (no motor index matches) | D06 all continue |
| FI11 | disabled; manipulate | ESC status unchanged | D09T |
| FI12 | enabled, no failures; manipulate | unchanged | D10F |
| FI13 | OFF(1) and STUCK(2) queued, one `update()` | both applied (loop processes queue) | D03 multi-iteration |
Sanitizer-only probes (NEVER in normal builds — undefined behaviour): `DISABLED_PRB05_EscCountAboveMax` (esc_count 9, STUCK active → ASan
out-of-bounds, F-07a) and `DISABLED_PRB06_NonMotorFunctionShift` (actuator_function 0 with STUCK → UBSan shift exponent, F-07b). See P11.

## Done when
All three binaries green; `--gtest_repeat=10 --gtest_shuffle` ×1 each stable (`tools/sqe_run_tests.sh shuffle`); both files at 100 % or gaps
documented (FI L43 first operand False infeasible; FD L194/L222 copy failures under investigation). Gate G09.
## Explain-back
Why FailureDetector needs the functional level; how the tests control time without lockstep; why the IMU tests need their own binary; how FD31
proves the injector is in the detector's control path.
