# P04 — Repository analysis + scope selection
**Goal:** a defensible, substantial business/control scope chosen from the production code, with dependencies and PX4 test level
justified per area. **Owner:** ANL. **Gate:** G04. **Rubric:** Part 1 (20 marks).

## Steps
1. **Inventory upstream tests** (what is already tested):
   `grep -rn "px4_add_unit_gtest\|px4_add_functional_gtest" --include=CMakeLists.txt PX4-Autopilot/src | sed -E 's/\s+/ /g' > work/scope/upstream_gtests.txt`
   (expect ≈119 registrations; none under `sensors/data_validator` or `commander/failure_detector`).
2. **Screen candidates** with the metric one-liner and read each candidate's code (see REF_02 for the pre-screened matrix):
   ```bash
   cd PX4-Autopilot/src && for f in <candidate .cpp files>; do echo "$f lines=$(wc -l <$f) decisions~=$(grep -cE '\bif\b|\bwhile\b|\bfor\b|\?|case ' $f) compound~=$(grep -cE '&&|\|\|' $f)"; done
   ```
3. **Verify the pre-selected areas A–D** (REF_02 §2) still hold: no upstream tests; non-trivial compound decisions; realistic setup cost;
   safety relevance traceable to call sites (`voted_sensors_update.cpp` get_best/failover_*, `VehicleMagnetometer.cpp`, `Commander.cpp`
   uses of `FailureDetector`). Commands: `grep -rn "get_best\|failover_index\|failover_state\|add_new_validator" PX4-Autopilot/src/modules/sensors`,
   `grep -rn "_failure_detector\|FailureDetector" PX4-Autopilot/src/modules/commander/*.cpp | head`.
4. **Decide test levels** (write the justification in SCOPE_RECORD, one paragraph each):
   - A/B unit: pure C++ logic; inputs are explicit (timestamps passed as arguments, error counts, priorities); no params, no uORB.
     Link-time needs platform symbols (`hrt_absolute_time`, `px4_log_raw` from `print()`), satisfied by linking `modules__sensors` —
     the same idiom upstream uses (`ManualControlSelectorTest` → `modules__manual_control`). Still a unit test: no runtime services used.
   - C/D functional: `DEFINE_PARAMETERS` (params read at construction), `SYS_FAILURE_EN` read in `FailureInjector()`, uORB subscriptions
     (vehicle_attitude, esc_status, pwm_input, sensor_selection, vehicle_imu_status multi-instance, actuator_motors, vehicle_command) and
     `hrt_absolute_time()`-based hysteresis/timeouts → requires `gtest_functional_main` (initialises uORB + params).
   - Why not SITL: no drivers, work queues, module start-up or simulator context is needed; SITL would add nondeterminism and cost without
     reaching additional decisions. (SITL already contributes incidental baseline coverage for A/B via `sitl-imu_filtering`.)
5. **Exclusions** (explicit, justified, never used to hide hard logic): header-only pure accessors in `*.hpp` (`sibling()`, `priority()`,
   `state()`, `failover_count()`, `getStatus()`, …); the orphaned legacy tests in `data_validator/tests/` (dead code, not built);
   `print()` functions are **kept in scope** for statement/branch coverage but labelled diagnostic (not business logic) and excluded from MC/DC.
6. **Write `work/scope/SCOPE_RECORD.md`** using the per-area template below (this becomes report §3 almost verbatim).
7. Fill `work/scope/CANDIDATES.md` (copy REF_02 matrix, re-verify every "verified" cell, add at least 2 candidates you inspected yourselves).

## Per-area template (SCOPE_RECORD.md)
```
### Area X — <file> (<class>)
Responsibility / critical behaviour: …
Key dependencies & state: inputs, internal state carried between calls, params, uORB topics, time source
Why non-trivial / why included: decisions (count), compound decisions, safety role (call sites)
PX4 test level + justification: …
Exclusions inside the file (if any) + justification: …
Baseline coverage (from evidence/coverage/baseline/per_file.md): line …%, branch …%
```
## Gate G04 checklist
- [ ] 4 areas documented with the template; level justification per area
- [ ] candidate matrix includes rejected options with reasons
- [ ] exclusions list reviewed by a human (HUMAN-DECISION D-004)
## Explain-back
Why these four files form a *substantial* scope; why unit vs functional; why SITL was unnecessary; what "safety-critical" means here
(voter selects which IMU feeds the EKF; failure detector feeds the failsafe state machine).
