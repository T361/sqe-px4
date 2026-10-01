# REF_07 — Candidate findings register (from static analysis; every item must be confirmed or rejected by execution — SPEC_10)
Nothing here is a confirmed defect until the evidence test ran **in the PX4 GTest suite** and SPEC_10 steps 1–6 were completed.
While preparing the kit, F-02, F-03, F-04, F-05, F-06, F-09, F-10, F-11, F-12 and F-13 were reproduced by compiling the v1.17.0 data_validator sources
standalone (GCC 13.3, stubs for logging/time). That pre-verification tells you the behaviour exists; your PX4 tests are the evidence you submit.

| ID | Title | Location | Evidence test(s) | Provisional class | Reachability / impact |
|---|---|---|---|---|---|
| F-01 | `best != nullptr` False branch infeasible | DVG L213 | proof + MC01 | structural observation (gap G-03) | none |
| F-02 | Total failure returns the failed sensor's last values with `index = -1` | DVG L234–245 | MC18, DVG11, PRB03 | API inconsistency vs header ("array of best values") | latent: current callers ignore the returned pointer |
| F-03 | `DataValidatorGroup(0)` + `add_new_validator()` dereferences null `_last` | DVG L67, L92 | DVG10 (death) | latent robustness (undocumented precondition); **pre-verified: SIGSEGV** | unreachable from production (`voter{1}`) |
| F-04 | Stale counter accumulates across axes; padded 1-D inputs (`[dp,0,0]`) hit the threshold after 35 equal samples instead of 100 | DV L82–87 | DV05 | specification ambiguity (documented in header) | airspeed validator (sensors.cpp:285) |
| F-05 | `print()` mutates the error mask (calls `confidence(hrt_absolute_time())`) | DV L152–153 | DV19 | diagnostic side effect | `sensors status` can set TIMEOUT flags on a stale validator |
| F-06 | 1 % equal-confidence test sits on the 1 % quantisation of confidence; outcome for adjacent density steps depends on float rounding **and on the optimisation level** | DVG L189 | measured with the real DataValidator code: E true for 84/100 adjacent steps; False at d = 3 8 16 21 29 32 42 45 53 59 65 71 78 84 90 96 at `-O0` (Coverage build), but at d = 3 9 16 21 30 33 43 46 54 60 67 73 80 86 92 99 at `-O2 -freciprocal-math` | specification ambiguity / evidence-transfer limitation | the measured (-O0) build and an optimised build can pick different sensors for the same inputs |
| F-07a | `manipulateEscStatus` loops to `esc_count` without the `CONNECTED_ESC_MAX` clamp FailureDetector uses | FI L117 | PRB05 (ASan only) | robustness defect (out-of-contract input) | only with failure injection active and esc_count > 8 |
| F-07b | `1 << i_esc` without range guard for non-motor `actuator_function` (unsigned wrap) | FI L120, L126 | PRB06 (UBSan only) | undefined behaviour | only with injection active + non-motor ESC function |
| F-08 | WRONG-type log message prints 0-based motor index | FI L95 | FI06 (+ optional capture) | cosmetic | log only |
| F-09 | Non-finite samples are silently ignored: a NaN-only stream keeps confidence 1.0 and NO_ERROR | DV L68, L100–142 | DV07, PRB01 | candidate defect vs documented purpose ("identify anomalies in data streams") | a sensor emitting NaN would stay "healthy"/selectable; check upstream NaN filtering (vehicle_imu) before rating severity |
| F-10 | Error density exactly at the window (100) → confidence 0 but no error flag (`>` vs implied `>=`) | DV L125 | DV15, PRB02 | boundary inconsistency | one-cycle "healthy" state with zero confidence (`accel_healthy` true) |
| F-11 | A confidence-driven switch between equal-priority healthy sensors is counted as a failover | DVG L207–227 | MC04, MC07 | classification semantics (ambiguity) | `failover_count()` increments; no message because `failover_index()` = −1 |
| F-12 | Timestamp 0 doubles as the "no data" sentinel | DV L69, L106 | DV20, MC17 | boundary observation | timestamps are never 0 after boot |
| F-13 | Constructor / `add_new_validator()` not robust to allocation failure (NuttX `new` returns null) | DVG L55–67, L86–92 | AF02 (optional) | latent, environment-dependent; **pre-verified:** failing first allocation → empty group; a later `add_new_validator()` → SIGSEGV | memory exhaustion at start-up |
| F-14 | `_first_failover_time` and `reset_state()` effects are not observable through the public API | DVG L214, L230–231 | analysis + O3 per-test coverage | testability observation | none (weak oracle only) |
| F-15 | Motor timed-out mask survives disarm (only under-current mask is cleared) | FD L345–353 | FD33 | observation (possible stale report after re-arm) | `getMotorFailures()` after disarm |

## How to confirm the main candidates
- **F-09:** run DV07; then read `src/modules/sensors/vehicle_imu/VehicleIMU.cpp` / `voted_sensors_update.cpp` to see whether NaN can reach `put()`;
  classify as defect only if the oracle (purpose statement) is accepted by the humans; otherwise "specification ambiguity with risk".
- **F-10:** DV15 shows conf 0 + state 0; show consumer impact: `voted_sensors_update.cpp:419` uses `state == NO_ERROR` as "healthy".
- **F-02:** MC18 shows non-null; confirm with the header text; note both callers ignore the pointer (`voted_sensors_update.cpp:195–196`,
  `VehicleMagnetometer.cpp:536`).
- **F-07:** only with `make tests PX4_ASAN=1` / `PX4_UBSAN=1` and the explicit probe run; never execute PRB05/06 in a normal build.
- **F-11:** present both readings (source comment "check whether the switch was a failsafe or preferring a higher priority sensor" vs counting
  all non-priority switches as failsafes) — do not call it a defect without a specification.
