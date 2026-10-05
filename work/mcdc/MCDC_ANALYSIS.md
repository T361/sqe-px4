# MCDC_ANALYSIS — DataValidatorGroup (P06)

Baseline: PX4-Autopilot v1.17.0 @ `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`. All line numbers, types, and constants
below were read directly from `PX4-Autopilot/src/modules/sensors/data_validator/DataValidatorGroup.cpp`,
`DataValidatorGroup.hpp`, `DataValidator.hpp`, and `DataValidator.cpp` in this session (P06), not copied from
`docs/reference/REF_06_MCDC_PREDERIVATION.md` without re-checking. Every row in `mcdc_matrix.csv` was independently
recomputed by hand (and cross-checked with short standalone Python arithmetic mirroring the exact C++ operator
precedence/short-circuit order) before being compared against REF_06. Method per `docs/specs/SPEC_04_MCDC_METHOD.md`.

## 1. Why `DataValidatorGroup` and what is safety-critical about it

`DataValidatorGroup` owns a linked list of `DataValidator` instances — one per physical sensor instance (e.g. IMU #0,
#1, #2) — and its `get_best()` method is the voting logic that decides **which physical sensor's data the EKF state
estimator actually uses on a given cycle**, based on each sensor's confidence (health, derived from timeout/
staleness/error density) and priority. `failover_index()`/`failover_state()` additionally tell the rest of the
system (logging, `vehicle_status`, ultimately operator/pilot awareness) which sensor failed and how. Confirmed call
sites (`work/scope/SCOPE_RECORD.md`, Area B): `modules/sensors/voted_sensors_update.cpp` (IMU accel/gyro voting) and
`modules/sensors/vehicle_magnetometer/VehicleMagnetometer.cpp` (magnetometer voting). A defect in this selection
logic — e.g. flapping between two sensors, failing to detect a dead sensor, or picking a worse sensor over a better
one — propagates directly into the attitude/position estimate, which is the textbook definition of safety-critical
for a flight controller. This is the justification already recorded in `work/scope/SCOPE_RECORD.md` Area B; this
document expands it for the MC/DC derivation specifically.

Among the four scoped areas (A: `DataValidator`, B: `DataValidatorGroup`, C: `FailureDetector`, D: `FailureInjector`),
B was chosen for the one required MC/DC component because it has the highest compound-decision density at the unit
level (5 genuinely compound Boolean decisions with 2–7 atomic conditions each, all pure functions of explicit
arguments and internal state — no params, no uORB, no hidden I/O) combined with the clearest, most direct safety
argument. `FailureDetector` (Area C) has more compound decisions in absolute count, but MC/DC there is explicitly
out of scope per `docs/plan` (D-003 in `work/DECISIONS.md`): the assignment requires MC/DC on **one** justified
component, and C is covered to decision/branch level only.

## 2. The 5 compound decisions needing MC/DC

All five were re-read directly from the live source this session (line numbers below are current, re-confirmed):

| ID | Location | Expression | Conditions |
|---|---|---|---|
| DVG-D13 | `DataValidatorGroup.cpp:187-190` | `((A&&B)\|\|(C&&D)\|\|(E&&F))&&G` | 7 |
| DVG-D14 | `DataValidatorGroup.cpp:203` | `H \|\| (I && J)` | 3 |
| DVG-D15 | `DataValidatorGroup.cpp:207-208` | `K && L && M` | 3 |
| DVG-D32 | `DataValidatorGroup.cpp:282-283` (`failover_index()`) | `P && Q && R` | 3 |
| DVG-D34 | `DataValidatorGroup.cpp:301-302` (`failover_state()`) | `P && Q && R` (same pattern as D32) | 3 |

### DVG-D13 — `get_best()` candidate-switch test, L187-190
```
if ((((max_confidence < MIN_REGULAR_CONFIDENCE) && (confidence >= MIN_REGULAR_CONFIDENCE)) ||
     (confidence > max_confidence && (next->priority() >= max_priority)) ||
     (fabsf(confidence - max_confidence) < 0.01f && (next->priority() > max_priority))) &&
    (confidence > 0.0f))
```
Conditions in evaluation (left-to-right, C++ short-circuit) order:
- A = `max_confidence < MIN_REGULAR_CONFIDENCE` (`MIN_REGULAR_CONFIDENCE` = `0.9f`, confirmed `DataValidatorGroup.hpp:147`)
- B = `confidence >= MIN_REGULAR_CONFIDENCE`
- C = `confidence > max_confidence`
- D = `next->priority() >= max_priority`
- E = `fabsf(confidence - max_confidence) < 0.01f`
- F = `next->priority() > max_priority`
- G = `confidence > 0.0f`

Evaluated once per sibling per call inside `get_best()`'s second `while` loop (confirmed in
`work/basis/CFG_DataValidatorGroup_get_best.md`, independently re-checked by reading L176-199 again this session).
`m`/`mp` below mean `max_confidence`/`max_priority` **at the moment of this particular evaluation** (they are mutated
across loop iterations as `best` is updated), `c`/`p` are the current sibling's `confidence(timestamp)`/`priority()`.

**Coupling (re-derived algebraically from the real operators, not assumed):**
- `F ⇒ D`: F is `p > mp`, D is `p >= mp`. `p > mp` implies `p >= mp` for any total order on integers. Real implication.
- `B ⇒ G`: B is `c >= 0.9f`, G is `c > 0.0f`. `c >= 0.9` implies `c > 0.0`. Real implication.
- `(A ∧ B) ⇒ C`: A is `m < 0.9f`, B is `c >= 0.9f`. If both hold, `c >= 0.9 > m`, so `c > m` = C. Real implication.

These three couplings make several condition combinations infeasible (see per-decision notes), and they are exactly
why REF_06's claim was trusted only after reproducing the algebra above independently.

**Short-circuit structure:** `||` of three `&&` pairs, then `&&` with G. If `A&&B` is true, `C`,`D`,`E`,`F` are never
evaluated (OR short-circuits). Else if `C&&D` is true (both evaluated), `E`,`F` are never evaluated. Else `E`,`F` are
evaluated. G is evaluated only if the inner `||` expression is true (AND short-circuits on a false left operand).
Full traces for all 12 evaluations are in `mcdc_matrix.csv`; recomputed independently with a small Python mirror of
the exact operator/short-circuit order (not just copied from REF_06) — all 12 traces matched REF_06 exactly,
including the non-obvious MC05 case where E *is* evaluated (because `C&&D` is false there, `D`=False) even though
the overall inner expression ends up false.

**Infeasible vectors** (same three found in REF_06, independently reconfirmed via the couplings above):
- `B=T ∧ G=F`: impossible because `B ⇒ G`.
- `F=T ∧ D=F`: impossible because `F ⇒ D`.
- `A=T ∧ B=T ∧ C=F`: impossible because `(A∧B) ⇒ C`.
No test is written for these vectors (SPEC_04 §3 step 4); they are excluded by construction, not by omission.

**Near-boundary vectors not used:** `C=T ∧ E=T` simultaneously (i.e. `0 < c−m < 0.01`) only occurs between adjacent
1% confidence quantisation steps, where float32 rounding of `1.0f − d/100.0f` decides the outcome (confirmed by
direct float32 arithmetic this session — see §4 below, F-06). None of the 12 chosen evaluations sit on such a
boundary; all `|c−m|` values used are ≥ 0.02 away from the `0.01f` threshold or ≥ 0.10 away from `0.9f`, confirmed by
direct computation (see `mcdc_matrix.csv` notes column per row).

### DVG-D14 — "did the best sensor change?" test, L203
```
if (max_index != _curr_best || ((max_confidence < FLT_EPSILON) && (_curr_best >= 0)))
```
H = `max_index != _curr_best` · I = `max_confidence < FLT_EPSILON` · J = `_curr_best >= 0`.
**Infeasible vector:** `H=F ∧ I=F ∧ J=F`. Proof (re-derived, not copied): `J=F` means `_curr_best < 0`, i.e.
`_curr_best == -1` (its only possible negative value, confirmed `DataValidatorGroup.hpp:140` `int _curr_best{-1}`
and the only writes to it are `_prev_best = max_index` / `_curr_best = max_index`, never any other negative).
With `_curr_best == -1` and `H=F` (`max_index == _curr_best == -1`), Loop 1 of `get_best()` never found a seed
(`pre_check_best == -1` is never matched by `i == pre_check_best` since `i` only takes values ≥ 0), so `best` stays
`nullptr` and, critically, `max_confidence` stays at its loop-entry initial value `-1.0f` (L149) **unless** Loop 2's
D13 fired for some sibling — but `max_index == -1` after Loop 2 means D13 never fired for any sibling (every
successful D13 hit sets `max_index = i ≥ 0`, L191). So `max_confidence == -1.0f < FLT_EPSILON` is forced, i.e. `I`
is forced **True**, contradicting `I=F`. Hence `H=F ∧ I=F ∧ J=F` is infeasible. (This matches REF_06 §2's
infeasibility note, independently re-derived with the extra step spelled out above.)

### DVG-D15 — "was the switch a real failsafe or a priority preference?" test, L207-208
```
if (pre_check_prio != -1 && pre_check_prio < max_priority &&
    fabsf(pre_check_confidence - max_confidence) < 0.1f)
```
Only reached when D14 is True. K = `pre_check_prio != -1` · L = `pre_check_prio < max_priority` ·
M = `fabsf(pre_check_confidence - max_confidence) < 0.1f`. `pre_check_confidence` defaults to `1.0f` (L147) when
Loop 1 never seeds it (i.e. when `_curr_best < 0`, same condition that forces `K=F`): confirmed by reading L146-174
again this session — `pre_check_prio`/`pre_check_confidence` are only ever written inside Loop 1's seed block
(L162-163), gated by `i == pre_check_best`, which is never true when `pre_check_best == _curr_best == -1`.
**Observability note (re-derived, see §4 of CFG doc and confirmed again here):** when `K=F`, `_curr_best` was `< 0`
at entry, which forces the `_curr_best < 0` branch at L219 regardless of D15's outcome — so D15's value has **no
observable effect on any getter** in that case (the `true_failsafe`/`best->reset_state()` path at L210-215 is simply
not reached when K=F, but the outer bookkeeping at L219-220 doesn't depend on `true_failsafe` either way). This K=F
row is therefore **O3** evidence (structural: show L210 was not executed in that test's per-test coverage), not O1/O2.

**Why no API-level oracle can exist for the K pair (post-audit addition).** D15's outcome has exactly two effects:
(1) `true_failsafe = false` (L210) and (2) `best->reset_state()` (L214). Effect (1) is read only at L226, on the
`_curr_best >= 0` path; K=False means `pre_check_prio == -1`, which only happens when `_curr_best == -1`, so L226 is
never reached in a K=False call and (1) cannot be seen. Effect (2) is a no-op in every reachable call: when D15 is
True, L requires `pre_check_prio < max_priority`, so `best` was selected in Loop 2 by D13, whose G condition means
`best->confidence(timestamp) > 0` was returned in this same call. `DataValidator::confidence()` sets
`_error_mask = ERROR_FLAG_NO_ERROR` whenever it returns a value > 0 (DataValidator.cpp L134-138), so `reset_state()`
writes the value the mask already has. With both effects unobservable, no getter can distinguish D15=True from
D15=False in the K pair, and per-test structural coverage is the strongest evidence that can exist.
**Reproduced independently:** running only MC01, then only MC21, with counters zeroed in between, gives L207 hits
2 / 1 (D15 evaluated in both), L210 hits 1 / 0, L214 hits 1 / 0 — D15 True in MC01, False in MC21.

### DVG-D32 / DVG-D34 — `failover_index()` / `failover_state()`, L282-283 / L301-302
```
if (next->used() && (next->state() != DataValidator::ERROR_FLAG_NO_ERROR) &&
    (i == (unsigned)_prev_best))
```
Identical 3-condition pattern in both functions (`failover_index()` returns `i`, `failover_state()` returns
`next->state()`), only the return value differs — same inputs, same truth table, re-derived once and reused for
both decisions in the matrix (12 rows total, 6 per decision, sharing the same 4 underlying scenarios).
P = `next->used()` · Q = `next->state() != ERROR_FLAG_NO_ERROR` · R = `i == (unsigned)_prev_best)`.
`_prev_best` is declared `int _prev_best{-1}` (`DataValidatorGroup.hpp:141`); `(unsigned)_prev_best` when
`_prev_best == -1` is `0xFFFFFFFF` (confirmed by direct computation: `(-1) & 0xFFFFFFFF == 0xffffffff` in Python,
mirroring C++ two's-complement unsigned conversion), which a real sibling index `i` (a small `unsigned`, bounded by
the number of siblings actually constructed) never equals — so `R` is always False whenever `_prev_best == -1`, as
REF_06 states.

## 3. Loops and state (SPEC_04 §4)

D13 is evaluated inside a `while` loop that mutates `max_confidence`/`max_priority`/`best` as it goes — each test
isolates **one target evaluation** by first calling `get_best()` once to seed a known current best, then introducing
the candidate sibling whose evaluation is under test on the next `get_best()` call. When the seeded best is
re-evaluated against itself on the second call, `B = ¬A` always (confidence hasn't changed, priority hasn't changed,
so either `A` was already true pre-call or `max_confidence` tracks exactly `confidence` → `C = (c>m) = (c>c) = F`
and `F = (p>mp) = (p>p) = F`), so the seeded sensor's own re-evaluation always yields decision=False and cannot
contaminate the candidate's evaluation that follows it in the same loop pass. This matches SPEC_04 §4's prescribed
design and REF_06's scenario construction (MC01-MC10 use exactly this seed-then-introduce-candidate pattern).

## 4. Float conditions (SPEC_04 §5, F-06)

Re-derived with direct float32 arithmetic this session (not assumed from REF_06): `confidence()` in `DataValidator.cpp`
returns `ret = 1.0f - (_error_density / ERROR_DENSITY_WINDOW)` where `ERROR_DENSITY_WINDOW = 100.0f` and
`_error_density` is an integer-valued `int` incremented by the delta in `error_count` across `put()` calls (confirmed
`DataValidator.cpp` L57-58, L125-129, L134). Computing `1.0f - d/100.0f` for integer `d` in Python (mirroring IEEE-754
float32, `struct.pack('f', ...)`) confirms: `d=10` → `0.89999998...f`, i.e. strictly **less than** `0.9f`, while the
*intended* value "0.90" would be exactly on the `MIN_REGULAR_CONFIDENCE` boundary — this is exactly REF_06's F-06
finding, independently reproduced. None of the 12 D13 test vectors in the matrix rely on a confidence value produced
by this `1 − d/100` formula landing exactly on a `0.9f` or `0.01f`/`0.1f` boundary; all confidence/priority literals
used are ≥ 0.02 away from the `0.01f` E-threshold and ≥ 0.01 away from the `0.9f` A/B-threshold measured in exact
float32 arithmetic (verified per-row, see notes column in `mcdc_matrix.csv`).

## 5. Observability classification (SPEC_04 §3 step 5)

- **O1** (direct return value): all DVG-D13 rows (`*index` after `get_best()`, confirmed via the returned pointer
  identity and `*index`), all DVG-D32/D34 rows (`failover_index()`/`failover_state()` return values directly), the
  DVG-D14 rows where the index value itself (e.g. `-1`) is the discriminator.
- **O2** (state getter): DVG-D14's H/I/J pairs and DVG-D15's L/M pairs, observed via `failover_count()` (a toggle
  is counted or not depending on `true_failsafe`, which these conditions gate).
- **O3** (structural coverage only, with per-test evidence artefact): DVG-D15's K=False row (MC13-equivalent) — its
  outcome has no API-visible effect (see §2 above), so the only evidence it was exercised as intended is a per-test
  coverage report showing line 210 (`true_failsafe = false;`) was **not** executed in that specific test, contrasted
  with a K=True test where it **was**. This requires `evidence/coverage/pertest/<test>.info` artefacts to be
  produced in P08/P10 when the tests are implemented; the matrix row notes this requirement explicitly.

## 6. Minimum vs used evaluation count (SPEC_04 §6)

D13 has 7 conditions → minimum 8 evaluations for unique-cause MC/DC; 12 are used (same count as REF_06) so that
every independence pair is individually explainable in the viva (see "Form of the independence pairs" below), per
SPEC_04 §6's stated rationale (traceability over minimality). D14/D15/D32/D34 each have 3 conditions → minimum 4
evaluations each; D14 uses 5 rows (REF_06's MC14/MC02/MC18c2/MC18c3/MC19 pattern, reused), D15 uses 4 rows, D32 and
D34 each use 4 rows (sharing the same 4 underlying scenarios MC14/MC15/MC16/MC17, observed through two different
getters).

**Form of the independence pairs (corrected wording, post-audit revision).** The pairs are unique-cause **with
short-circuit relaxation**, not strict unique-cause: within each pair the target condition flips, the outcome flips,
and every other condition that is *evaluated in both tests* holds the same value — but a condition can be evaluated
in one test and short-circuited (`NE(x)` in `mcdc_matrix.csv`) in the other. Example: the A pair MC07/MC08 — in MC07
`A&&B` is true so C is never evaluated; in MC08 A is false so C is evaluated (True). No pair relies on a condition
that is evaluated in both tests changing value, so none needs masking MC/DC. Earlier wording ("unique-cause
throughout, no masking", "differs in exactly one input dimension") overstated this, because strict unique-cause
would require every other condition to be identical, including the short-circuited ones.

## 7a. Test ID / gtest mapping (SPEC_01 §3) — for P08 implementation

`mcdc_matrix.csv` uses 22 distinct test IDs (`SQE-DVG-MC-01` … `SQE-DVG-MC-25`, with gaps where a scenario was
dropped during derivation) across 29 target-evaluation rows — several IDs cover more than one decision row because a
single `get_best()`/`failover_index()`/`failover_state()` call sequence naturally produces more than one target
evaluation (e.g. `SQE-DVG-MC-01`'s call sequence evaluates both DVG-D13 at i=1 and, in the same call, DVG-D15; the
`SQE-DVG-MC-13` scenario's three `get_best()` calls plus a `failover_index()`/`failover_state()` pair evaluate
DVG-D14, DVG-D32 and DVG-D34). Per SPEC_01 §3 each test ID maps to one `TEST_F(SqeDvgMcdcTest, MCnn_<name>)` in
fixture `SqeDvgMcdcTest`; where one test ID produces multiple matrix rows (multiple decisions/evaluations), the P08
implementation is one gtest function containing multiple assertions (one per evaluation it exercises), which is
consistent with "one behaviour per test" in CLAUDE.md §5 only if that behaviour is read as "this scenario's full
`get_best()` call sequence" — each individual assertion still traces to exactly one decision ID/evaluation via the
`Structural Coverage Target` column of the (future) test inventory row, as required by SPEC_05 §1 col 7. These test
IDs do not exist as code yet (P08 implements them); this matrix is what P08 implements against.

## 7. Cross-check verdict vs REF_06

Every logical value, short-circuit trace, outcome, coupling claim (F⇒D, B⇒G, (A∧B)⇒C), and infeasibility proof in
REF_06 §1-§4 was independently recomputed this session (hand algebra plus a standalone Python mirror of the exact
C++ operator/short-circuit order for D13, D14, D32/D34; direct inspection for D15) and **all values matched
exactly** — no discrepancies found. The one place this analysis goes beyond REF_06 is spelling out the D14
infeasibility proof's missing step (why `I` is forced True when `H=F ∧ J=F`) and making the D15 K=False → O3
reasoning fully explicit (REF_06 states the conclusion; here it is re-derived from L146-174/L203-220 directly).
REF_06's self-reported "pre-verified by execution" claim (standalone compile of the real source) was **not**
independently re-executed in this session (no standalone harness was built) — the matrix values were verified by
hand/script arithmetic against the real source and operator semantics, which is sufficient for the written MC/DC
derivation; actual GTest execution against the real PX4 build happens in P08 and will be the final confirmation.

**One genuine correction found while deriving the DVG-D32/D34 scenarios** (not present in REF_06, which does not
spell out `_prev_best` bookkeeping across calls): on the very first `get_best()` call ever made (`_curr_best < 0` at
entry), `DataValidatorGroup.cpp:220` sets `_prev_best = max_index` — **not** `pre_check_best`, which is only used on
the `else` branch (`DataValidatorGroup.cpp:224`, "we were initialized before, this is a real failsafe"). An earlier
draft of this matrix's `SQE-DVG-MC-24` scenario incorrectly assumed `_prev_best` stayed at its construction value
`-1` through a sequence that started with a bootstrap call; re-reading `DataValidatorGroup.cpp:218-224` directly
caught this, and the scenario/notes were corrected before the row was finalised (the final `R` logical value, `1 ==
(unsigned)_prev_best`, is still False either way — with `_prev_best=0` from the corrected reading, `1 != 0`; with the
originally-assumed `_prev_best=-1` cast to `0xFFFFFFFF`, `1 != 0xFFFFFFFF` — so the row's T/F content was unaffected,
but the explanatory note would have been factually wrong about *why*, which R3/R4 do not allow even when harmless to
the checker's pass/fail result).
