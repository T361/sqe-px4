# Decision inventory — land detector (Area F, PX4 v1.17.0 @ d6f12ad1)

Files: `src/modules/land_detector/LandDetector.cpp` (base class: work-queue cycle, publication, flight time, at-rest)
and `src/modules/land_detector/MulticopterLandDetector.cpp` (multicopter decision logic). Tests: `SQE-MLD-nn`,
`SQE-MLD-MC-nn` (`SqeMcLandDetectorTest.cpp`), `SQE-LD-nn` (`SqeLandDetectorRunTest.cpp`), `SQE-LDB-01`
(`SqeLandDetectorBootTest.cpp`).

**Responsibility / critical behaviour.** Decides ground contact → maybe-landed → landed (three hysteresis stages),
free fall, ground effect and at-rest, and publishes `vehicle_land_detected`. Commander uses `landed` for auto-disarm
and take-off/landing transitions, so a false "landed" in flight can disarm the motors and a missed "landed" keeps
them spinning on the ground; the flight-time parameters are written on disarm.

**Dependencies.** Parameters `MPC_THR_MIN/THR_HOVER/MANTHR_MIN/USE_HTE/LAND_SPEED/LAND_CRWL`, `LNDMC_TRIG_TIME/ROT_MAX/
XY_VEL_MAX/Z_VEL_MAX/ALT_GND`, `LND_FLIGHT_T_HI/LO`; uORB inputs `actuator_armed`, `vehicle_acceleration`,
`vehicle_angular_velocity`, `vehicle_local_position`, `vehicle_status`, `vehicle_thrust_setpoint`,
`vehicle_control_mode`, `hover_thrust_estimate`, `takeoff_status`, `trajectory_setpoint`, `sensor_selection`,
`vehicle_imu_status`, `parameter_update`; output `vehicle_land_detected`; wall-clock time (hysteresis, data age).

**Test levels.** `MulticopterLandDetector` decisions: GTest functional with a test subclass (seam) that re-exports the
protected decision methods and sets the protected inputs Run() would fill; hysteresis driven with explicit timestamps.
`LandDetector::Run()` is private and inherited through a private base, so it is executed through PX4's real work-queue
manager (`WorkQueueManagerStart()`, `start()`), observed through uORB. SITL was not needed: no simulator dynamics are
involved in any decision.

LandDetector.cpp: 122 executable lines, 20 decisions (8 compound: D01, D06, D08, D09, D10, D11, D16, D19), 144
source-level branch outcomes.
MulticopterLandDetector.cpp: 116 executable lines, 39 decisions (15 compound: D09, D13, D14, D15, D17, D20, D25, D26,
D27, D28, D29, D34, D35, D36, D37), 156 source-level branch outcomes.

## LandDetector.cpp

| ID | Line | Expression | Covered by |
|---|---|---|---|
| LD-D01 | 83 | `param_update.updated() \|\| timestamp == 0` | op1 T: every first cycle, LD09 · op2 T: LDB01 · F: later cycles |
| LD-D02 | 96 | `_actuator_armed_sub.update()` | T: LD03 · F: idle cycles |
| LD-D03 | 102 | `_vehicle_acceleration_sub.update()` | T: LD01, LD04 · F: idle cycles |
| LD-D04 | 113 | `‖ω‖ > 3 deg/s` | T/F: LD07 |
| LD-D05 | 123 | `!_dist_bottom_is_observable` | T then F: LD08, LD11 |
| LD-D06 | 130 | `observable && !dist_valid` | T: LD08 · T∧F: LD11 · op1 F: LD01 |
| LD-D07 | 108 | `_vehicle_angular_velocity_sub.update()` | T: LD07 · F: idle cycles |
| LD-D08 | 153 | `landDetected && _at_rest` | T/F: LD03, LD06 |
| LD-D09 | 156-162 | publish if ≥1 s or any of 6 outputs changed | op1 T: LD01, LD02 · op2 T: LD03 · op3 T: LD04 · op4 T: LD03 (landing) · op5 T: LD03 · op6 T: LD05 · op7 T: LD06 · all F: LD02 |
| LD-D10 | 164 | `!landed && was_landed && takeoff_time == 0` | T: LD03 · op2 F: LD04 · op3 F: LD03 (2nd take-off) |
| LD-D11 | 187 | `takeoff_time != 0 && !armed && prev_armed` | T: LD03 · op1 F: LD01 · op2 F: LD03 (armed) · **op3 F infeasible (G-10)** |
| LD-D12 | 206 | `should_exit()` | T: LD10 (+ every test's stop) · F: all cycles before |
| LD-D13 | 214 | `_sensor_selection_sub.updated()` | T: LD06, LD07, LD12 · F: idle cycles |
| LD-D14 | 218 | `gyro id != _device_id_gyro` | T: LD06 · F: LD06 (same id re-published) |
| LD-D15 | 223 | `imu_instance < 4` | T/F: LD07 (search exhausts all 4) |
| LD-D16 | 229 | `id != 0 && id == selected` | T: LD06, LD12 · op1 F: LD07 · op2 F: LD12 |
| LD-D17 | 237 | `!gyro_status_found` | T: LD07 · F: LD06 |
| LD-D18 | 245 | `_vehicle_imu_status_sub.update()` | T/F: LD06, LD12 |
| LD-D19 | 249-250 | `gyro_vib > 0.02 \|\| accel_vib > 1.2` | op1 T, op2 T, F: LD06 |
| LD-D20 | 256 | `elapsed(last_move) > 1 s` | T/F: LD06, LD07 |

## MulticopterLandDetector.cpp

| ID | Line | Expression | Covered by |
|---|---|---|---|
| MLD-D01/02/06 | 91/97/114 | thrust / control-mode / takeoff `update()` | T: MLD03, MLD15, MLD24 · F: MLD24 (2nd call) |
| MLD-D03..05 | 101/104/105 | `useHTE`, `hte update`, `hte.valid` | T: MLD04 · D03 F: MLD24 · D04 F, D05 F: MLD05 |
| MLD-D07 | 129 | `LNDMC_Z_VEL_MAX > min(crawl,land)/1.2` | T (clamped + committed): MLD02 · F: MLD01 |
| MLD-D08/09 | 139/141 | `use_hte == 1`; `!useHTE \|\| !initialised` | op1 T: MLD01 · op2 T, F∨F: MLD04 |
| MLD-D10 | 156 | `‖a‖ < 2` | T/F incl. boundary 2.0: MLD08 |
| MLD-D11 | 163-165 | position recent (< 1 s) | T: most · F: MLD09 |
| MLD-D12 | 173 | landed → vertical threshold ×2.5 | T/F: MLD12 |
| MLD-D13 | 177 | `v_z_valid && \|vz\| < thr` | T, F incl. boundary: MLD10 · op1 F: MLD11 |
| MLD-D14 | 180 | `z_valid && \|z_deriv\| < thr` | T/F: MLD11 · op1 F: MLD10 |
| MLD-D15/16 | 194/196 | `recent && v_xy_valid`; `‖v_xy‖ > max` | T/F: MLD13 · op1 F: MLD09 |
| MLD-D17/18 | 202/203 | `recent && dist_valid && ALT_GND > 0`; `dist < ALT_GND` | MLD14 (incl. boundary 2.0 m, ALT_GND 0) · op1 F: MLD09 |
| MLD-D19 | 209 | HTE recent (< 1 s) | T: MLD04 · F: MLD06 |
| MLD-D20 | 211 | `!in_descend \|\| hte_valid` | op1 T: MLD06, MLD25 · F∨T, F∨F: MLD07 |
| MLD-D21/22 | 217/219 | 0.6 vs 0.3 factor; `throttle <= low` | MLD03 (boundary), MLD04 |
| MLD-D23/24 | 224/227 | climb-rate control; trajectory `update()` | T: MLD15 · D23 F: MLD18 · D24 F: MLD17 |
| MLD-D25 | 229-230 | `finite(vz_sp) && vz_sp >= 1.1*Z_VEL_MAX` | op1 F, T∧F, T∧T: MLD15 · boundary: MLD16 |
| MLD-D26 | 234 | `!maybe && !landed` | T: MLD15 · op1 F, op2 F: MLD17 |
| MLD-D27/28 | 245/246 | skip = `!observable \|\| !dist_valid`; `close \|\| skip` | MLD19 |
| **MLD-D29** | 249-251 | `!armed \|\| (close_or_skip && gc && !horiz && !vert)` | **MC/DC: MLD-MC-01..06** |
| MLD-D30/31 | 260/268 | climb-rate thrust limit; `throttle <= limit` | MLD20 · manual: MLD-MC-08/09 |
| MLD-D32/33 | 276/280 | landed → rotation threshold ×2.5; `‖ω_xy‖ > thr` | MLD21, MLD-MC-11 |
| MLD-D34 | 283-285 | `recent && v_z_valid` | T: MLD-MC-09 · F: MLD-MC-13, MLD21 |
| **MLD-D35** | 287-290 | `!armed \|\| (min_thr && !ff && !rot && ((vert && gc) \|\| (!vert && min8s)))` | **MC/DC: MLD-MC-07..14** |
| MLD-D36 | 296 | `!armed \|\| maybe_landed` | MLD22 |
| **MLD-D37** | 301-303 | `(descend && !horiz) \|\| (below && FLIGHT) \|\| RAMPUP` | **MC/DC: MLD-MC-15..21** |
| MLD-D38/39 | 308/309 | `dist_valid`; `dist < 1.0` | MLD19 (incl. boundary 1.0 m) |
