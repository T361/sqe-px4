# Uncovered items — `evidence/coverage/IT-1/scope.info`
Classify each: missing-test | infeasible | environment-limited | tool-artefact (SPEC_10 §4)

## FailureDetector.cpp
- [ ] G-? L46 branches missed 19/38 [be0.1, be0.3, be0.5, be0.7, be0.9, be0.11, be0.13, be0.15, be0.17, be0.19, be0.21, be0.23, be0.25, be0.27, be0.29, be0.31, be0.33, be0.35, be0.37]: `ModuleParams(parent)`
- [ ] G-? L52 branches missed 1/2 [be0.1]: `_failure_injector.update();`
- [ ] G-? L57 branches missed 1/2 [be0.1]: `updateAttitudeStatus(vehicle_status);`
- [ ] G-? L60 branches missed 1/2 [be0.1]: `updateExternalAtsStatus();`
- [ ] G-? L73 branches missed 1/4 [be0.1]: `if (_esc_status_sub.update(&esc_status)) {`
- [ ] G-? L74 branches missed 1/2 [be0.1]: `_failure_injector.manipulateEscStatus(esc_status);`
- [ ] G-? L77 branches missed 1/2 [be0.1]: `updateEscsStatus(vehicle_status, esc_status);`
- [ ] G-? L81 branches missed 1/2 [be0.1]: `updateMotorStatus(vehicle_status, esc_status);`
- [ ] G-? L86 branches missed 1/2 [be0.1]: `updateImbalancedPropStatus();`
- [ ] G-? L96 branches missed 1/4 [be0.1]: `if (_vehicle_attitude_sub.update(&attitude)) {`
- [ ] G-? L98 branches missed 2/4 [be0.1, be0.3]: `const matrix::Eulerf euler(matrix::Quatf(attitude.q));`
- [ ] G-? L99 branches missed 1/2 [be0.1]: `float roll(euler.phi());`
- [ ] G-? L100 branches missed 1/2 [be0.1]: `float pitch(euler.theta());`
- [ ] G-? L111 branches missed 5/10 [be0.1, be0.3, be0.5, be0.7, be0.9]: `const matrix::Eulerf euler_rotated = matrix::Eulerf(matrix::Quatf(attitude.q) * matrix::Quatf(matrix::Eulerf(0.f,`
- [ ] G-? L112 branches missed 1/2 [be0.1]: `M_PI_2_F, 0.f)));`
- [ ] G-? L113 branches missed 1/2 [be0.1]: `roll = euler_rotated.phi();`
- [ ] G-? L114 branches missed 1/2 [be0.1]: `pitch = euler_rotated.theta();`
- [ ] G-? L126 branches missed 1/2 [be0.1]: `hrt_abstime time_now = hrt_absolute_time();`
- [ ] G-? L129 branches missed 1/2 [be0.1]: `_roll_failure_hysteresis.set_hysteresis_time_from(false, (hrt_abstime)(1_s * _param_fd_fail_r_ttri.get()));`
- [ ] G-? L130 branches missed 1/2 [be0.1]: `_pitch_failure_hysteresis.set_hysteresis_time_from(false, (hrt_abstime)(1_s * _param_fd_fail_p_ttri.get()));`
- [ ] G-? L131 branches missed 1/2 [be0.1]: `_roll_failure_hysteresis.set_state_and_update(roll_status, time_now);`
- [ ] G-? L132 branches missed 1/2 [be0.1]: `_pitch_failure_hysteresis.set_state_and_update(pitch_status, time_now);`
- [ ] G-? L144 branches missed 1/4 [be0.1]: `if (_pwm_input_sub.update(&pwm_input)) {`
- [ ] G-? L149 branches missed 1/2 [be0.1]: `hrt_abstime time_now = hrt_absolute_time();`
- [ ] G-? L152 branches missed 1/2 [be0.1]: `_ext_ats_failure_hysteresis.set_hysteresis_time_from(false, 100_ms); // 5 consecutive pulses at 50hz`
- [ ] G-? L153 branches missed 1/2 [be0.1]: `_ext_ats_failure_hysteresis.set_state_and_update(ats_trigger_status, time_now);`
- [ ] G-? L161 branches missed 1/2 [be0.1]: `hrt_abstime time_now = hrt_absolute_time();`
- [ ] G-? L174 branches missed 1/2 [be0.1]: `_esc_failure_hysteresis.set_hysteresis_time_from(false, 300_ms);`
- [ ] G-? L175 branches missed 1/2 [be0.1]: `_esc_failure_hysteresis.set_state_and_update(is_esc_failure, time_now);`
- [ ] G-? L183 branches missed 1/2 [be0.1]: `_esc_failure_hysteresis.set_state_and_update(false, time_now);`
- [ ] G-? L191 branches missed 1/4 [be0.1]: `if (_sensor_selection_sub.updated()) {`
- [ ] G-? L194 branches missed 2/4 [be0.1, b0.3]: `if (_sensor_selection_sub.copy(&selection)) {`
- [ ] G-? L199 branches missed 1/2 [be0.1]: `const bool updated = _vehicle_imu_status_sub.updated(); // save before doing a copy`
- [ ] G-? L203 branches missed 1/2 [be0.1]: `_vehicle_imu_status_sub.copy(&imu_status);`
- [ ] G-? L208 branches missed 1/4 [be0.1]: `if (!_vehicle_imu_status_sub.ChangeInstance(i)) {`
- [ ] G-? L212 branches missed 1/2 [be0.1]: `if (_vehicle_imu_status_sub.copy(&imu_status)`
- [ ] G-? L213 branches missed 1/6 [b0.1]: `&& (imu_status.accel_device_id == _selected_accel_device_id)) {`
- [ ] G-? L222 branches missed 2/4 [be0.1, b0.3]: `if (_vehicle_imu_status_sub.copy(&imu_status)) {`
- [ ] G-? L229 branches missed 1/2 [be0.1]: `_imbalanced_prop_lpf.setParameters(dt, _imbalanced_prop_lpf_time_constant);`
- [ ] G-? L237 branches missed 1/2 [be0.1]: `const float metric_lpf = _imbalanced_prop_lpf.update(metric);`
- [ ] G-? L260 branches missed 1/2 [be0.1]: `const hrt_abstime now = hrt_absolute_time();`
- [ ] G-? L264 branches missed 1/2 [be0.1]: `_actuator_motors_sub.copy(&actuator_motors);`
- [ ] G-? L313 branches missed 1/6 [b0.5]: `if (throttle_above_threshold && current_too_low && !esc_timed_out) {`
- [ ] G-? L326 branches missed 1/6 [b0.3]: `&& (_motor_failure_esc_under_current_mask & (1 << i_esc)) == 0) {`

## FailureInjector.cpp
- [ ] G-? L43 branches missed 2/4 [be0.1, be0.3]: `if ((param_get(param_find("SYS_FAILURE_EN"), &param_sys_failure_en) == PX4_OK)`
- [ ] G-? L44 branches missed 1/6 [b0.1]: `&& (param_sys_failure_en == 1)) {`
- [ ] G-? L55 branches missed 1/4 [be0.1]: `while (_vehicle_command_sub.update(&vehicle_command)) {`
- [ ] G-? L75 branches missed 1/2 [be0.1]: `PX4_INFO("CMD_INJECT_FAILURE, motor %d ok", i + 1);`
- [ ] G-? L83 branches missed 1/2 [be0.1]: `PX4_INFO("CMD_INJECT_FAILURE, motor %d off", i + 1);`
- [ ] G-? L89 branches missed 1/2 [be0.1]: `PX4_INFO("CMD_INJECT_FAILURE, motor %d no esc telemetry", i + 1);`
- [ ] G-? L95 branches missed 1/2 [be0.1]: `PX4_INFO("CMD_INJECT_FAILURE, motor %d esc telemetry wrong", i);`
- [ ] G-? L107 branches missed 1/2 [be0.1]: `ack.timestamp = hrt_absolute_time();`
- [ ] G-? L108 branches missed 1/2 [be0.1]: `_command_ack_pub.publish(ack);`

## DataValidator.cpp — no gaps

## DataValidatorGroup.cpp
- [ ] G-? L89 line not executed: `return nullptr;`
- [ ] G-? L55 branches missed 1/2 [b0.1]: `next = new DataValidator();`
- [ ] G-? L78 branches missed 1/2 [b0.1]: `delete (_first);`
- [ ] G-? L86 branches missed 1/2 [b0.1]: `DataValidator *validator = new DataValidator();`
- [ ] G-? L88 branches missed 1/2 [b0.0]: `if (!validator) {`
- [ ] G-? L213 branches missed 1/2 [b0.1]: `if (best != nullptr) {`
- [ ] G-? L260 branches missed 1/12 [b0.10]: `PX4_INFO_RAW("sensor #%u, prio: %d, state:%s%s%s%s%s%s\n", i, next->priority(),`

Total uncovered items: 60
