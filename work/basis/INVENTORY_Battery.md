# Decision inventory — `src/lib/battery/battery.cpp` (Area E, PX4 v1.17.0 @ d6f12ad1)

Derived from the live source before the tests were written; line numbers are v1.17.0. "T/F" = decision outcome,
"op k" = k-th atomic operand of a compound decision. Test IDs are `SQE-BAT-nn` in `src/lib/battery/SqeBatteryTest.cpp`.

**Responsibility.** Turns raw voltage/current samples into the `battery_status` message: connection/initialisation
state, voltage-based and coulomb-counted state of charge, the LOW/CRITICAL/EMERGENCY warning that drives Commander's
low-battery failsafe, an online internal-resistance estimator (RLS), over-voltage fault, voltage scale and remaining
flight time.

**Dependencies.** Parameters `BATn_V_EMPTY/V_CHARGED/N_CELLS/CAPACITY/R_INTERNAL/SOURCE` (n = 1..3 only, module.yaml
`num_instances: 3`) and `BAT_LOW_THR/CRIT_THR/EMERGEN_THR/AVRG_CURRENT`, read in the constructor and in `updateParams()`
(protected); uORB `vehicle_status` (armed, fixed-wing) and `flight_phase_estimation` (level flight) read in
`computeRemainingTime()`; `battery_status` published. All other inputs are explicit arguments and timestamps.

**Test level.** GTest functional (`px4_add_functional_gtest`): parameters and uORB are required. Test doubles:
`TestableBattery` (seam re-exporting protected `updateParams()`); link-time fake of `uORB::Manager::orb_data_copy`
for BAT-D26 False.

231 executable lines, 35 decisions, 13 compound, 179 source-level branch outcomes (lcov, exception edges excluded).

| ID | Line | Expression | Outcomes / operands covered by |
|---|---|---|---|
| BAT-D01 | 53 | `index < 1 \|\| index > 9 ? 1 : index` | op1 T: BAT01(0) · op2 T: BAT01(10) · F: BAT02, BAT03 |
| BAT-D02 | 61 | `index > 9 \|\| index < 1` | T/T: BAT01 · F: BAT02 |
| BAT-D03 | 71 | `v_empty == PARAM_INVALID` | T: BAT03 (index 4) · F: all others |
| BAT-D04 | 119 | `_voltage_v < 2.1` | T: BAT04(2.0) · F incl. boundary 2.1: BAT04 |
| BAT-D05 | 123 | `!_connected \|\| last_unconnected == 0` | op1 T: BAT06 · op2 T: BAT05 first sample · F: BAT05 later samples |
| BAT-D06 | 128 | `_connected && t > last + 2 s` | F (boundary t = last+2 s): BAT05 · T: BAT05 (+1 us) · op1 F: BAT06 |
| BAT-D07 | 130 | `_connected && !init && ir_init && n_cells > 0` | all T: BAT07 · op1 F: BAT06 · op2 F: BAT05 (initialised) · op3 F: BAT10, BAT28 · **op4 F infeasible (G-09)** |
| BAT-D08 | 138 | `!_external_state_of_charge` | T: BAT07 · F: BAT16 |
| BAT-D09 | 144 | `_connected && _battery_initialized` | T: BAT05 · op1 F: BAT06 · op2 F: BAT05 |
| BAT-D10 | 172 | `n_cells > 0 ? interpolate : -1` | T: BAT07 · F: BAT03, BAT10 |
| BAT-D11 | 183 | `_source == _params.source` | T/F: BAT17 |
| BAT-D12 | 195 | `_last_timestamp != 0` | F/T: BAT14 |
| BAT-D13 | 204 | `_dt > FLT_EPSILON` | F/T: BAT14 |
| BAT-D14 | 215 | `n_cells == 0` | T: BAT10, BAT28 · F: BAT07 |
| BAT-D15 | 223 | `current_a > FLT_EPSILON` | T: BAT07 · F: BAT09 |
| BAT-D16 | 226 | `r_internal >= 0` | T: BAT07, boundary 0: BAT31 · F: BAT08 |
| BAT-D17 | 253 | `cov_norm_new < cov_norm` (RLS accept) | T and F: BAT29 (behavioural oracle: converges to the synthetic pack) |
| BAT-D18 | 279 | `r_internal >= 0` (estimator reset) | T: BAT07, BAT31 · F: BAT08 |
| BAT-D19 | 290 | `capacity > 0 && initialised` | T∧F, T∧T: BAT15 · F: BAT07 |
| BAT-D20..22 | 309/312/315 | `soc < EMERGEN / CRIT / LOW` | T/F incl. exact thresholds: BAT13; through the pipeline: BAT05 |
| BAT-D23 | 327-328 | `n_cells > 0 && V > n*Vch*1.05` | T∧T / T∧F: BAT12 · op1 F: BAT10 |
| BAT-D24 | 340 | `PX4_ISFINITE(_scale)` | T: BAT11 · F: BAT10 |
| BAT-D25 | 353 | `_vehicle_status_sub.updated()` | T: BAT18 · F: BAT20 (2nd call) |
| BAT-D26 | 356 | `_vehicle_status_sub.copy()` | T: BAT18 · **F: BAT26 (orb_data_copy fake)** |
| BAT-D27 | 359 | `type == FW && !_vehicle_status_is_fw` | T then F: BAT23 · op1 F: BAT20 |
| BAT-D28 | 370-371 | `!finite(avg) \|\| avg < eps \|\| reset` | op1 T: BAT25 · op2 T: BAT18 · op3 T: BAT23 · all F: BAT20 (2nd call) |
| BAT-D29 | 375 | `_armed && finite(current)` | T∧T: BAT20 · F: BAT18 · T∧F: BAT22 |
| BAT-D30 | 377-378 | `!is_fw \|\| (recent && LEVEL)` | op1 T: BAT20 · F∨(T∧T): BAT24 · F∨(F∧·): BAT24 · F∨(T∧F): BAT24 |
| BAT-D31 | 379 | `_dt > FLT_EPSILON` | T: BAT20 · F: BAT21 |
| BAT-D32 | 389 | `capacity > 0` | T: BAT18 · F: BAT19 |
| BAT-D33 | 400 | `_first_parameter_update ? 0 : n` | T: constructor (all) · F: BAT25, BAT27 |
| BAT-D34 | 415 | `n_cells != _params.n_cells` | T: BAT27 (4→6, 6→0) · F: BAT25, BAT27 (unchanged) |
| BAT-D35 | 419 | `!ir_init && n_cells > 0` | T: constructor, BAT27 · op1 F: BAT27 (unchanged) · op2 F: BAT03, BAT27 (0 cells) |

Compound decisions (13): D01, D02, D05, D06, D07, D09, D19, D23, D27, D28, D29, D30, D35.
