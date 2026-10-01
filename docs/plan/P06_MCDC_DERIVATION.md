# P06 — MC/DC derivation for the critical component (DataValidator + DataValidatorGroup)
**Goal:** a complete, verified MC/DC matrix for the compound decisions that implement redundant-sensor selection and failover
classification, ready to be implemented as tests. **Owner:** MCDC. **Gate:** G06. **Spec:** SPEC_04 (binding). **Pre-derivation:** REF_06.

## 1. Component, critical behaviour, decisions (write in `work/mcdc/MCDC_ANALYSIS.md`)
- **Component:** `data_validator` library = `DataValidator` (per-sensor confidence) + `DataValidatorGroup` (selection/failover), a
  cohesive unit used by `VotedSensorsUpdate` (accel, gyro voters) and `VehicleMagnetometer` (mag voter).
- **Why critical:** the group decides which physical IMU/magnetometer instance feeds the estimator; a wrong decision keeps a failed sensor in
  the control loop or triggers needless switches; `failover_count()/failover_index()/failover_state()` drive the "sensor failure" emergency
  event and the priority demotion of the failed sensor in `VotedSensorsUpdate::checkFailover()`.
- **Critical behaviour analysed:** (i) choosing the best sensor, (ii) deciding that a selection change happened or that the only sensor failed,
  (iii) classifying the change as a true failover vs a priority switch, (iv) reporting the failed sensor.
- **MC/DC decisions (non-trivial compound):**
  | ID | Line | Decision | Conds |
  |---|---|---|---|
  | DVG-D13 | 187–190 | `((A∧B) ∨ (C∧D) ∨ (E∧F)) ∧ G` switch-to-candidate | 7 |
  | DVG-D14 | 203 | `H ∨ (I ∧ J)` selection changed or only sensor went bad | 3 |
  | DVG-D15 | 207–208 | `K ∧ L ∧ M` priority switch (not a failover) | 3 |
  | DVG-D32 | 282–283 | `P ∧ Q ∧ R` failover_index | 3 |
  | DVG-D34 | 301–302 | `P ∧ Q ∧ R` failover_state | 3 |
  Single-condition decisions (e.g. `DataValidator::confidence` else-if chain) are covered by decision coverage; MC/DC = DC for them (state this).

## 2. Steps
1. Re-derive the condition definitions from the source (REF_06 §1) and confirm the coupling facts: F ⇒ D; B ⇒ G; (A ∧ B) ⇒ C; for the
   seeded current best compared with itself the decision is always False (B = ¬A, C = F, F = F).
2. For every row in REF_06 §3–§6: compute each condition's **logical value** from the concrete inputs (confidence = 1 − density/100 with
   density = first error_count; priorities from `put()`), then the **evaluation trace** (T/F/NE under `&&`/`||` short-circuit), then the outcome.
   Recompute the float values that matter (0.89 < 0.9f, 0.91 ≥ 0.9f, |0.97−0.95| ≥ 0.01f, |0.95−0.97| < 0.1f …). Avoid pairs that sit on the
   1 % quantisation boundary (REF_07 F-06: `< 0.01f` at exact 1 % steps is True for 84/100 adjacent pairs, False for 16 and flag-dependent).
3. Fill `work/mcdc/mcdc_matrix.csv` (schema SPEC_05 §3). One row per **target evaluation**; `NE(T)` / `NE(F)` records "not evaluated,
   logical value T/F". Every condition must have an independence pair; prefer **unique-cause** (only the target condition's logical value
   differs); use **masking** only with a written argument.
4. Classify observability for each row (O1 return value, O2 state getter, O3 only via per-test structural evidence) — REF_06 §7.
   DVG-D15 condition K can only be demonstrated with O3 (K = False implies the initialisation path where `true_failsafe` is discarded).
5. Run `python3 tools/sqe_mcdc_check.py work/mcdc/mcdc_matrix.csv` until it prints `ALL DECISIONS COMPLETE`.
   It checks: pairs exist, outcomes differ, target condition differs, unique-cause vs masking, each condition T and F, each decision T and F.
6. Optional tool cross-check (GCC ≥ 14 only; does not replace the derivation): see SPEC_04 §8 (`-fcondition-coverage` + `gcov --conditions`).

## 3. Output
`work/mcdc/MCDC_ANALYSIS.md` (justification + one worked pair per decision in prose), `work/mcdc/mcdc_matrix.csv` (→ workbook Sheet 2).

## Gate G06 checklist
- [ ] checker passes; every row mapped to a test ID from the P08 catalogue (MC01–MC20)
- [ ] criticality + critical behaviour paragraphs reviewed by a human
- [ ] infeasible vectors documented, none turned into fake tests
## Explain-back
Explain the E pair (MC01 vs MC03) and the K pair (MC01 vs MC13) aloud: which inputs change, which conditions change, why the outcome flips,
and how you *observe* the outcome.
