# SETUP_MAP — how each scoped component's decisions are controlled (P05)

Verified against the live v1.17.0 source (`PX4-Autopilot/src/modules/sensors/data_validator/*.hpp`,
`PX4-Autopilot/src/modules/commander/failure_detector/*.hpp`).

| Control | How (verified API) |
|---|---|
| `DataValidator` state | `put(timestamp, val[3] or scalar, error_count, priority)` drives DV-D01..D06; `confidence(timestamp)` drives DV-D07..D13 and is also the only caller of `_time_last`/`_error_count`/`_error_density`/`_value_equal_count` reads. `set_timeout()`/`set_equal_value_threshold()` (inherited setters, not shown in REF_03 but used by `DataValidatorGroup`) change the thresholds DV-D08/D09 compare against. All inputs are explicit function arguments — no hidden global/static state. |
| Group topology | `DataValidatorGroup(unsigned siblings)` ctor builds an `siblings`-length linked list (DVG-D01/D02/D03); `add_new_validator()` appends one more (DVG-D05); `put(index, timestamp, val, error_count, priority)` routes to the `index`-th validator (DVG-D08/D09). The "current best" (`_curr_best`, init −1) is only updated by `get_best()`, so a test must call `get_best()` at least once before `_curr_best` reflects anything — the first call is special-cased by D16/D17 (`_curr_best < 0`). |
| Params (Area C/D) | Per CLAUDE.md §5 and SPEC_02: fixture must call `param_control_autosave(false)` + `param_reset_all()` in `SetUp()`, then `param_set(param_find("FD_FAIL_R"), &v)` etc. **before** constructing `FailureDetector(ModuleParams *parent)` — params are read once via `DEFINE_PARAMETERS` at construction (confirmed `FailureDetector.hpp:129-138`), not re-read per call. Same applies to `FailureInjector()`'s `SYS_FAILURE_EN` read (`param_get` at construction, confirmed `FailureInjector.cpp:43`). |
| uORB inputs | `uORB::Publication<T>`/`PublicationMulti<T>` must publish **before** the first `update()`/subscriber-driven call, since `uORB::Subscription::update()`/`updated()` only sees messages published before the call and PX4's uORB has latest-sample (not queued-history) semantics for single-instance topics. Confirmed subscriptions in `FailureDetector.hpp`: `vehicle_attitude`, `esc_status`, `pwm_input`, `sensor_selection`, `vehicle_imu_status` (multi-instance, needs `uORB::SubscriptionMultiArray` / `ChangeInstance()` per FD-D23/D24/D25), `actuator_motors`; `FailureInjector.hpp`: `vehicle_command`. |
| Vehicle state (Area C) | `vehicle_status_s` and `vehicle_control_mode_s` structs are passed **directly as arguments** to `FailureDetector::update(vehicle_status, vehicle_control_mode)` — not read via uORB inside `update()` itself (confirmed signature at `FailureDetector.hpp:84`). A test constructs these structs in-line and sets the fields each decision needs (e.g. `flag_control_attitude_enabled`, `arming_state`, `is_vtol_tailsitter`). |
| Time (Areas C/D) | Real `hrt_absolute_time()` is read internally by `FailureDetector`/`FailureInjector` (confirmed e.g. `FailureDetector.cpp:126,149,161,260`) — no lockstep in `px4_sitl_test`, so hysteresis-driven decisions (FD-D19 ESC hysteresis 300ms, attitude hysteresis per `FD_FAIL_R_TTRI`/`FD_FAIL_P_TTRI`, ATS hysteresis 100ms) need the test to actually sleep/wait real wall-clock time, using the documented minimum trigger times with a ≥ 1.5× safety margin (R11) rather than a fixed short sleep that could flake. |
| Hysteresis internals | `Hysteresis::set_hysteresis_time_from(false, duration)` + `set_hysteresis_time_from(...).set_state_and_update(bool, now)` — the state only flips after `duration` of the new boolean input being continuously true/false. A test must call `update()` (or the sub-function) repeatedly across real elapsed time, not just once, to observe a hysteresis-gated transition. |

## Why this matters for test design
- **A/B (unit)**: every input is an explicit argument. A test is just "call `put()`/`confidence()`/`get_best()` in a
  specific sequence with specific values and assert the return/state." No fixture complexity beyond constructing the
  object(s).
- **C/D (functional)**: three ordering constraints must all be respected or the test is meaningless (R11, R4): params
  set → object constructed → uORB published → `update()` called. Getting any of these out of order means the test
  exercises default/stale state instead of the intended decision, which would be an oracle that silently tests the
  wrong thing.
