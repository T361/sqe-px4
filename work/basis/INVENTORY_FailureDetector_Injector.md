> **P05 verification note (2026-10-01):** spot-checked line-by-line against the live source at
> `PX4-Autopilot/src/modules/commander/failure_detector/{FailureDetector,FailureInjector}.cpp` on commit
> `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`: FD-D01/D03/D04/D05/D06/D08/D09/D10/D11/D12 (lines 56-123) confirmed
> exact. Both flagged defects independently re-verified by reading the real code: **F-07a/F-07b** — FI-D11 (L117,
> `i < status.esc_count`) genuinely has no clamp to `CONNECTED_ESC_MAX`(8) unlike the sibling loop at L67, and
> FI-D12 (L120, `_esc_telemetry_blocked_mask & (1 << i_esc)`) genuinely shifts by an unguarded `i_esc` derived from
> `actuator_function - ACTUATOR_FUNCTION_MOTOR1` with no range check — confirmed real, not a documentation error.
> **F-08** — FI-D07 (L72-98 switch): every case logs `i + 1` except `FAILURE_TYPE_WRONG` (L95) which logs plain `i`,
> confirmed a genuine 1-indexing inconsistency in the log message only (the actual bitmask logic at L96 still
> correctly uses `1 << i`). All remaining rows (FD-D13 through FD-D45, lines 123-347) also individually re-walked
> against the live source this session and confirmed exact — every line number and condition matches. Copied here
> (verbatim) from `docs/reference/REF_05_INVENTORY_FAILUREDETECTOR_INJECTOR.md` as the P05 working basis.

# REF_05 — Decision inventory: `FailureDetector.cpp` + `FailureInjector.cpp` (commander/failure_detector, v1.17.0)
Params (defaults / range): FD_FAIL_R 60 (0–180°), FD_FAIL_P 60, FD_FAIL_R_TTRI 0.3 (0.02–5 s), FD_FAIL_P_TTRI 0.3, FD_EXT_ATS_EN 0,
FD_EXT_ATS_TRIG 1900 µs, FD_ESCS_EN 1, FD_IMB_PROP_THR 30 (0–1000), FD_ACT_EN 1, FD_ACT_MOT_THR 0.2, FD_ACT_MOT_C2T 2.0 A/%, FD_ACT_MOT_TOUT 100 ms
(10–10000), SYS_FAILURE_EN 0 (system_params.c:303). Fixed times: ESC hysteresis 300 ms, ATS hysteresis 100 ms, ESC telemetry timeout 300 ms.
Inputs: `update(vehicle_status, vehicle_control_mode)` structs + topics vehicle_attitude, pwm_input, esc_status, sensor_selection,
vehicle_imu_status (multi), actuator_motors, vehicle_command (injector). Outputs: `getStatus()/getStatusFlags()`, `getMotorFailures()`,
`getImbalancedPropMetric()`, return value of `update()` (status changed), vehicle_command_ack (injector), `getMotorStopMask()`.

## FailureDetector.cpp
| ID | Line | Code | Notes | Tests |
|---|---|---|---|---|
| FD-D01 | 56 | `flag_control_attitude_enabled` | F resets roll/pitch/alt/ext | FD01 |
| FD-D02 | 59 | `_param_fd_ext_ats_en.get()` | | FD11,13 |
| FD-D03 | 73 | `_esc_status_sub.update(&esc_status)` | F: second update without publish | FD15,22 |
| FD-D04 | 76 | `_param_escs_en.get()` | | FD15,21 |
| FD-D05 | 80 | `_param_fd_actuator_en.get()` | | FD23,30 |
| FD-D06 | 85 | `_param_fd_imb_prop_thr.get() > 0` | | FDI01,03 |
| (ret) | 89 | `_status.value != status_prev.value` | return value, not a branch | FD02,08,32 |
| FD-D08 | 96 | `_vehicle_attitude_sub.update(&attitude)` | | FD02,08 |
| FD-D09 | 103 | `is_vtol_tailsitter` | | FD09,10 |
| FD-D10 | 104 | `in_transition_mode` | T: roll=pitch=0 | FD09,10 |
| FD-D11 | 109 | `vehicle_type == FIXED_WING` | T: rotate +90° pitch | FD10 |
| FD-D12 | 123 | `(max_roll > FLT_EPSILON) && (fabsf(roll) > max_roll)` | 1st F when FD_FAIL_R 0 | FD02,03,04 |
| FD-D13 | 124 | pitch analogue | | FD05–07 |
| FD-D14 | 144 | `_pwm_input_sub.update(&pwm_input)` | | FD11,14 |
| FD-D15 | 147 | `(pulse_width >= trig) && (pulse_width < 3_ms)` | window [trig, 3000 µs) | FD11,12 |
| FD-D16 | 163 | `arming_state == ARMED` (ESC) | F: reset arm_escs | FD15,19 |
| FD-D17 | 170 | `i < limited_esc_count` | count clamped to 8 | FD15,20 |
| FD-D18 | 171 | `is_esc_failure || (esc[i].failures > 0)` | | FD15,16,17 |
| FD-D19 | 177 | `_esc_failure_hysteresis.get_state()` | latched while armed (no else) | FD16,18 |
| FD-D20 | 191 | `_sensor_selection_sub.updated()` | | FDI01,07 |
| FD-D21 | 194 | `_sensor_selection_sub.copy(&selection)` | F likely infeasible after updated() (investigate, G-07) | FDI01 |
| FD-D22 | 205 | `accel_device_id != _selected_accel_device_id` | T: search instances | FDI01,04 |
| FD-D23 | 207 | `i < ORB_MULTI_MAX_INSTANCES` (10) | | FDI04,05 |
| FD-D24 | 208 | `!ChangeInstance(i)` | T for non-existing instances | FDI05 |
| FD-D25 | 212–213 | `copy(&imu_status) && (id == selected)` | T: break | FDI04,05 |
| FD-D26 | 220 | `updated` | saved before instance search | FDI01,07 |
| FD-D27 | 222 | `copy(&imu_status)` | F: investigate (G-07) | FDI01 |
| FD-D28 | 224–225 | `(id != 0) && (id == selected)` | | FDI01,05,06 |
| FD-D29 | 259 | `arming_state == ARMED` (motor) | F: reset under-current, motor flag | FD23,29,33 |
| FD-D30 | 267 | `esc_status_idx < limited_esc_count` | | FD23 |
| FD-D31 | 274 | `i_esc >= NUM_CONTROLS` | T for non-motor functions (unsigned wrap) | FD28 |
| FD-D32 | 279 | `!(valid_mask & bit) && esc_current > 0.0f` | | FD23,24 |
| FD-D33 | 288 | `esc_was_valid && esc_timed_out && !flagged` | 3 conds | FD23,24 |
| FD-D34 | 292 | `!esc_timed_out && flagged` | clears timed-out bit | FD23 |
| FD-D35 | 298 | `esc_current > FLT_EPSILON` | | FD24,25 |
| FD-D36 | 302 | `_motor_failure_esc_has_current[i_esc]` | | FD24,25 |
| FD-D37 | 305 | `PX4_ISFINITE(control[i_esc])` | F: NaN → throttle 0 | FD25,27 |
| FD-D38 | 313 | `throttle_above && current_too_low && !esc_timed_out` | 3 conds | FD25,26 |
| FD-D39 | 314 | `start_time == 0` | | FD25 |
| FD-D40 | 319 | `start_time != 0` | reset timer | FD26 |
| FD-D41 | 324–326 | `start != 0 && now > start + tout && !(mask & bit)` | 3 conds; latched ("never cleared") | FD25 |
| FD-D42 | 334 | `timed_out_mask != 0 || under_current_mask != 0` | boolean expr with short-circuit edges | FD23,25 |
| FD-D43 | 336 | `critical && !motor_flag` | set flag | FD23 |
| FD-D44 | 340 | `!critical && motor_flag` | reset flag | FD23 |
| FD-D45 | 347 | `i_esc < NUM_CONTROLS` (disarmed loop) | | FD29,33 |

## FailureInjector.cpp
| ID | Line | Code | Notes | Tests |
|---|---|---|---|---|
| FI-D01 | 43–44 | `param_get(...) == PX4_OK && value == 1` | 1st operand F infeasible in this build (G-04) | FI01,02 |
| FI-D02 | 51 | `!_failure_injection_enabled` | early return | FI01 |
| FI-D03 | 55 | `while (_vehicle_command_sub.update(&cmd))` | multiple iterations with queued commands | FI02,13 |
| FI-D04 | 59–60 | `command != INJECT_FAILURE || unit != SYSTEM_MOTOR` | continue | FI08,09 |
| FI-D05 | 67 | `i < CONNECTED_ESC_MAX` | bounded (8) | FI02,03 |
| FI-D06 | 68 | `instance != 0 && i != (instance - 1)` | | FI02,03,10 |
| FI-D07 | 72 | `switch (failure_type)` OK/OFF/STUCK/WRONG/no-match | 5 gcov edges; WRONG log prints `i` not `i+1` (F-08) | FI02,04,05,06,07 |
| FI-D08 | 104–106 | `supported ? ACCEPTED : UNSUPPORTED` | | FI02,07,10 |
| FI-D09 | 114 | `!_failure_injection_enabled` | | FI11 |
| FI-D10 | 116 | `blocked_mask != 0 || wrong_mask != 0` | | FI05,12 |
| FI-D11 | 117 | `i < status.esc_count` | **no clamp to 8** (F-07a) | FI05 |
| FI-D12 | 120 | `blocked_mask & (1 << i_esc)` | shift unguarded for non-motor functions (F-07b) | FI05 |
| FI-D13 | 126 | `wrong_mask & (1 << i_esc)` | | FI06 |
