# REF_06 — MC/DC pre-derivation for DataValidatorGroup (v1.17.0) — re-derive and verify in P06

> **Pre-verified by execution while preparing the kit:** the v1.17.0 `DataValidator.cpp` + `DataValidatorGroup.cpp` were compiled standalone
> (GCC 13.3, `-O0`, PX4 math flags, stubbed `PX4_INFO_RAW`/`hrt_absolute_time`) and every scenario MC01–MC20, DVG07 and the DV boundary cases in
> P07 were run: all observed indices, counts, `failover_index/state` values and flags matched the expected results below. The matrix in
> `templates/mcdc/mcdc_matrix.PREDERIVED.csv` passes `tools/sqe_mcdc_check.py`. This is **not** a substitute for the PX4 GTest suite — re-run everything in PX4.
Notation: logical values T/F computed from inputs; trace entries `NE(x)` = not evaluated (short-circuit), logical value x.
m = `max_confidence`, mp = `max_priority` at the moment of the evaluation (seeded from the current best in loop 1, or −1 / −1000 if none);
c, p = candidate's `confidence(timestamp)` and `priority()`. confidence = 1 − d/100 where d = error_count of the validator's first put.

## 1. DVG-D13 (L187–190) `((A && B) || (C && D) || (E && F)) && G`
A = `m < 0.9f` · B = `c >= 0.9f` · C = `c > m` · D = `p >= mp` · E = `fabsf(c − m) < 0.01f` · F = `p > mp` · G = `c > 0.0f`
**Coupling:** F ⇒ D · B ⇒ G · (A ∧ B) ⇒ C · for the seeded current best evaluated against itself: B = ¬A, C = F, F = F ⇒ decision F
(so in two-sensor scenarios the candidate's evaluation at i = 1 decides the outcome).
**Infeasible vectors (examples):** B=T ∧ G=F · F=T ∧ D=F · A=T ∧ B=T ∧ C=F. Near-boundary vectors (C=T ∧ E=T, i.e. 0 < c − m < 0.01) exist only
between adjacent 1 % confidence steps where float rounding decides (F-06) → deliberately not used.

| Test (eval) | m / mp | c / p | A | B | C | D | E | F | G | Trace | Out |
|---|---|---|---|---|---|---|---|---|---|---|---|
| MC01 (call2,i=1) | .95/50 | .95/75 | F | T | F | T | T | T | T | A F · B NE(T) · C F · D NE(T) · E T · F T · G T | **T** |
| MC02 (call2,i=1) | .95/50 | .95/50 | F | T | F | T | T | F | T | A F · B NE(T) · C F · D NE(T) · E T · F F · G NE(T) | F |
| MC03 (call2,i=1) | .95/50 | .93/75 | F | T | F | T | F | T | T | A F · B NE(T) · C F · D NE(T) · E F · F NE(T) · G NE(T) | F |
| MC04 (call2,i=1) | .95/50 | .97/50 | F | T | T | T | F | F | T | A F · B NE(T) · C T · D T · E NE(F) · F NE(F) · G T | **T** |
| MC05 (call2,i=1) | .95/50 | .97/25 | F | T | T | F | F | F | T | A F · B NE(T) · C T · D F · E F · F NE(F) · G NE(T) | F |
| MC06 (call2,i=1) | .95/50 | .93/50 | F | T | F | T | F | F | T | A F · B NE(T) · C F · D NE(T) · E F · F NE(F) · G NE(T) | F |
| MC07 (call2,i=1) | .89/50 | .95/25 | T | T | T | F | F | F | T | A T · B T · C…F NE · G T | **T** |
| MC08 (call2,i=1) | .91/50 | .95/25 | F | T | T | F | F | F | T | A F · B NE(T) · C T · D F · E F · F NE(F) · G NE(T) | F |
| MC09 (call2,i=1) | .80/50 | .95/25 | T | T | T | F | F | F | T | A T · B T · C…F NE · G T | **T** |
| MC10 (call2,i=1) | .80/50 | .85/25 | T | F | T | F | F | F | T | A T · B F · C T · D F · E F · F NE(F) · G NE(T) | F |
| MC11 (call1,i=0) | −1/−1000 | 0 (no data)/0 | T | F | T | T | F | T | F | A T · B F · C T · D T · E NE(F) · F NE(T) · G F | F |
| MC12 (call1,i=0) | −1/−1000 | .50/50 | T | F | T | T | F | T | T | A T · B F · C T · D T · E NE(F) · F NE(T) · G T | **T** |
**Independence pairs (all unique-cause on logical values):** A: MC07/MC08 · B: MC09/MC10 · C: MC04/MC06 · D: MC04/MC05 · E: MC01/MC03 ·
F: MC01/MC02 · G: MC12/MC11. Each condition is T and F across the set; the decision is T (MC01,04,07,09,12) and F (others).
Minimum would be 8 evaluations (n+1); 12 are used so that every pair differs in exactly one input.
Float checks (float32): 0.89 < 0.9f ✓ · 0.91 ≥ 0.9f ✓ · 0.85 < 0.9f ✓ · |0.97−0.95| ≈ 0.0200 ≥ 0.01 ✓ · |0.93−0.95| ≈ 0.0200 ✓ · |0.95−0.95| = 0 ✓.

## 2. DVG-D14 (L203) `max_index != _curr_best || ((max_confidence < FLT_EPSILON) && (_curr_best >= 0))` — H ∨ (I ∧ J)
| Test (eval) | Situation | H | I | J | Trace | Out | Observed as |
|---|---|---|---|---|---|---|---|
| MC14 (call3) | switch 0→1 after sensor 0 timed out | T | F | T | H T · I NE(F) · J NE(T) | **T** | count 1 (with D15 F), idx 1 |
| MC02 (call2) | stable, current best keeps winning | F | F | T | H F · I F · J NE(T) | F | count 0, idx 0 |
| MC18 (call2) | single sensor, fresh data | F | F | T | H F · I F · J NE(T) | F | count 0, idx 0 |
| MC18 (call3) | single sensor silent > timeout | F | T | T | H F · I T · J T | **T** | count 1, idx −1 |
| MC19 (call1) | single sensor never fed | F | T | F | H F · I T · J F | F | count 0, idx −1, nullptr |
Pairs: H: MC14c3/MC02c2 · I: MC18c3/MC18c2 · J: MC18c3/MC19c1 (all unique-cause). Infeasible: H F ∧ I F ∧ J F (J F ⇒ no current best ⇒ max_index
= −1 ⇒ m = −1 ⇒ I T).

## 3. DVG-D15 (L207–208) `pre_check_prio != -1 && pre_check_prio < max_priority && fabsf(pre_check_confidence - max_confidence) < 0.1f` — K ∧ L ∧ M
Evaluated only when D14 is T. `pre_check_confidence` defaults to 1.0f when there is no current best.
| Test (eval) | pre prio / max prio | pre conf / max conf | K | L | M | Trace | Out | Observed as |
|---|---|---|---|---|---|---|---|---|
| MC01 (call2) | 50 / 75 | .95 / .95 | T | T | T | K T · L T · M T | **T** | switch, count stays 0 (O2) |
| MC04 (call2) | 50 / 50 | .95 / .97 | T | F | T | K T · L F · M NE(T) | F | count 1 (O2) — F-11 |
| MC20 (call2) | 50 / 75 | .50 / .95 | T | T | F | K T · L T · M F | F | count 1 (O2) |
| MC13 (call1) | −1 / 50 | 1.0 / .95 | F | T | T | K F · L NE(T) · M NE(T) | F | **O3** only: L210 not executed (per-test coverage) |
Pairs: K: MC01/MC13 · L: MC01/MC04 · M: MC01/MC20 (unique-cause). K=F implies `_curr_best < 0` → initial bookkeeping path where
`true_failsafe` is discarded, so D15's outcome has no API-visible effect in that case → O3 evidence (compare L210/L213/L214 hit counts of the
per-test runs of MC01 [≥1] and MC13 [0]).

## 4. DVG-D32 (L282–283, `failover_index`) and DVG-D34 (L301–302, `failover_state`) — P ∧ Q ∧ R
P = `next->used()` · Q = `next->state() != NO_ERROR` · R = `i == (unsigned)_prev_best` (`(unsigned)-1` never equals i).
| Test (eval) | Situation | P | Q | R | Trace | Out | Observed as |
|---|---|---|---|---|---|---|---|
| MC14 (i=0) | after failover; sensor 0 timed out, prev best 0 | T | T | T | P T · Q T · R T | **T** | index 0 / flags TIMEOUT |
| MC15 (i=0) | sensor 0 recovered (confidence cleared its mask) | T | F | T | P T · Q F · R NE(T) | F | −1 / NO_ERROR |
| MC16 (i=1) | sensor 1 timed out but never best; prev best 0 | T | T | F | P T · Q T · R F | F | −1 / NO_ERROR |
| MC17 (i=0) | after failover, `put(0, t = 0, …)` resets used() | F | T | T | P F · Q NE(T) · R NE(T) | F | −1 / NO_ERROR |
Pairs: P: MC14/MC17 · Q: MC14/MC15 · R: MC14(i=0)/MC16(i=1) (unique-cause). MC17 uses a timestamp of 0: code-feasible but domain-invalid
(real sensor timestamps are never 0 after boot) — say so in the matrix notes (lecture 8: code-feasible vs domain-valid).
The same four scenarios give D34's rows (outcome observed through `failover_state()`).

## 5. Observability summary
O1: D13 (index), D32/D34 (return values), D14 rows with index −1 · O2: D14 H/I pairs and D15 L/M pairs via `failover_count()` · O3: D15 K pair.

## 6. Scenario setup reference (all timestamps µs, T0 = 1'000'000, default timeout 40 000)
MC01–MC10: `g(2)`; `put(0,T0,V,d0,p0)`; `get_best(T0)` → 0; `put(1,T0+1000,V,d1,p1)`; `get_best(T0+1000)`.
MC11: `g(2)`; `get_best(T0)`. MC12/13: `g(2)`; `put(0,T0,V,d0,50)`; `get_best(T0)`.
MC14: `g(2)`; `put(0,T0,V,0,50)`; `get_best(T0)`; `put(1,T0+1000,V,0,50)`; `get_best(T0+1000)` (=0); `put(1,T0+50000,V2,0,50)`; `get_best(T0+50000)` (=1).
MC15: MC14 + `put(0,T0+60000,V,0,50)`; `put(1,T0+60000,V2,0,50)`; `get_best(T0+60000)`.
MC16: `g(2)`; `put(0,T0,V,0,50)`; `put(1,T0,V,0,40)`; `get_best(T0)` (=0); `put(0,T0+50000,V2,0,50)`; `get_best(T0+50000)`.
MC17: MC14 + `put(0, 0, V, 0, 50)` then `failover_index()`, `failover_state()`.
MC18: `g(1)`; `put(0,T0,V,0,50)`; `get_best(T0)`; `put(0,T0+1000,V2,0,50)`; `get_best(T0+1000)`; `get_best(T0+41001+1)`.
MC19: `g(1)`; `get_best(T0)`. MC20: `g(2)`; `put(0,T0,V,50,50)`; `get_best(T0)`; `put(1,T0+1000,V,5,75)`; `get_best(T0+1000)`.
Consecutive puts to the same validator must use different vectors (V, V2) only where stale detection could matter (it cannot with ≤ 3 puts and
threshold 100, but keep it explicit).
