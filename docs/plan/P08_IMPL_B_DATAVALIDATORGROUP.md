# P08 — Implementation chunk B: `DataValidatorGroup` (GTest unit, MC/DC component)
**Goal:** structural tests for every decision in `DataValidatorGroup.cpp` + the executable MC/DC suite MC01–MC20 from P06.
**Owner:** IMPL (+MCDC review). **Gate:** G08. **Inventory:** REF_04. **MC/DC rows:** REF_06 / `work/mcdc/mcdc_matrix.csv`.

## Files
`PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp` (skeleton in templates/test_skeletons/), registered as in P07.
Fixtures: `SqeDvgTest` (structural), `SqeDvgMcdcTest` (MC/DC), `SqeDvgDeathTest` (death tests run first by gtest convention).

## Scenario recipe used by MC01–MC10 (two sensors, sensor 0 = current best)
```
DataValidatorGroup g(2); int idx = -99;
g.put(0, T0, V1, d0, p0);            // first put: density = d0 → confidence c0 = 1 − d0/100
g.get_best(T0, &idx);   ASSERT_EQ(idx, 0);          // establishes _curr_best = 0 (sensor 1 has no data → conf 0)
g.put(1, T0 + 1000, V1, d1, p1);     // candidate: c1 = 1 − d1/100 (first put)
g.get_best(T0 + 1000, &idx);         // target evaluation of DVG-D13 at i = 1 (i = 0 self-comparison is always False)
```
Precondition checks inside each test: `get_sensor_priority(0/1)`, `get_sensor_state(0/1)==0` for healthy sensors, `idx==0` after step 1.
Oracle: `idx` (O1) and `failover_count()` (O2). Densities: 0.98→2, 0.97→3, 0.95→5, 0.93→7, 0.91→9, 0.89→11, 0.85→15, 0.80→20, 0.50→50.

## MC/DC catalogue (IDs `SQE-DVG-MC-nn` → gtest `MCnn_…`)
| ID | c0/p0 → c1/p1 (or setup) | Expected | Target evaluations (REF_06) |
|---|---|---|---|
| MC01 | 0.95/50 → 0.95/75 | idx 1, failover_count 0 | D13=T (E,F); D14=T (H); D15=T (K,L,M) |
| MC02 | 0.95/50 → 0.95/50 | idx 0, count 0 | D13=F (F pair of MC01); D14=F (H pair) |
| MC03 | 0.95/50 → 0.93/75 | idx 0 | D13=F (E pair of MC01) |
| MC04 | 0.95/50 → 0.97/50 | idx 1, count **1** (F-11) | D13=T (C,D); D15=F (L pair of MC01) |
| MC05 | 0.95/50 → 0.97/25 | idx 0 | D13=F (D pair of MC04) |
| MC06 | 0.95/50 → 0.93/50 | idx 0 | D13=F (C pair of MC04) |
| MC07 | 0.89/50 → 0.95/25 | idx 1, count 1 | D13=T (A,B via A∧B) |
| MC08 | 0.91/50 → 0.95/25 | idx 0 | D13=F (A pair of MC07) |
| MC09 | 0.80/50 → 0.95/25 | idx 1, count 1 | D13=T |
| MC10 | 0.80/50 → 0.85/25 | idx 0 | D13=F (B pair of MC09) |
| MC11 | fresh g(2), no data | idx −1, `get_best` returns nullptr, count 0 | D13=F at i=0 (G=F); D14=F (J=F) |
| MC12 | fresh g(2); sensor 0 0.50/50 only | idx 0 | D13=T at i=0 (G pair of MC11) |
| MC13 | fresh g(2); sensor 0 0.95/50 only | idx 0, count 0; **O3:** per-test coverage shows L207 executed, L210 not executed | D15=F (K pair of MC01) |
| MC14 | classic failover: 1.0/50 both; sensor 0 silent > 40 ms, sensor 1 fresh | idx 1, count 1, `failover_index()`=0, `failover_state()`=TIMEOUT | D13=T (A,B); D14=T (H); D15=F; D32/D34=T at i=0 |
| MC15 | MC14 then both fresh again | idx 1 (no switch back), `failover_index()`=−1, `failover_state()`=0 | D14=F; D32/D34=F at i=0 (Q) |
| MC16 | sensor 0 best & fresh; sensor 1 had data once, now timed out | idx 0, `get_sensor_state(1)`=TIMEOUT, `failover_index()`=−1, `failover_state()`=0 | D32/D34=F at i=1 (R) |
| MC17 | MC14 then `put(0, 0 /*t=0*/, …)` | `get_sensor_state(0)` still TIMEOUT, `failover_index()`=−1, `failover_state()`=0 (domain-invalid timestamp 0) | D32/D34=F at i=0 (P) |
| MC18 | g(1): select; fresh call; then silence > 40 ms | after fresh call idx 0 count 0; after silence idx −1, count 1, return **non-null** (F-02 characterization), `failover_index()`=0 | D14=F (I pair) then D14=T (H=F,I=T,J=T) |
| MC19 | g(1) never fed | idx −1, nullptr, count 0 | D14=F (J pair of MC18) |
| MC20 | 0.50/50 → 0.95/75 | idx 1, count 1 | D15=F (M pair of MC01) |

## Structural catalogue (IDs `SQE-DVG-nn` → gtest `DVGnn_…`)
| ID | Scenario | Expected | Decisions |
|---|---|---|---|
| DVG01 | g(1): queries on index 0 and 1 | prio(0)=0, state(0)=0, state(1)=UINT32_MAX, prio(1)=0, count 0, failover_index −1, failover_state 0 | D01 D02T D03T D35 D36T/F D37 D38T/F D31 D33 |
| DVG02 | g(3): `put(2,…,5,42)`, `put(7,…)` | prio(2)=42, prio(0)=prio(1)=0; out-of-range put ignored | D02F D08 D09T/F (loop exit w/o break) |
| DVG03 | g(0): get_best, failover_*, get_sensor_*, set_timeout, set_equal_value_threshold, print | idx −1 + nullptr; −1; 0; UINT32_MAX; 0; no crash | D01 0-iter, D03F, D06/D07/D10/D12 0-iter |
| DVG04 | g(1): default then `set_timeout(250000)`; `add_new_validator()` | returned ptr ≠ nullptr; `get_timeout()` 40000 before / 250000 after; state(1) ≠ UINT32_MAX | D05F D06 |
| DVG05 | g(2) `set_timeout(10000)`; data at T0; `get_best(T0+10001)` | idx −1; state(0), state(1) have TIMEOUT | D06 |
| DVG06 | g(2) `set_equal_value_threshold(2)`; constant {1,1,1} ×2 each | both STALE, idx −1 | D07 |
| DVG07 | fresh g(2): 1.0/10 and 1.0/20 at T0 | idx 1 (equal confidence → higher priority) | D13=T twice (i=0 via A∧B, i=1 via E∧F) |
| DVG08 | after MC14: back-and-forth second failover | count 2; `failover_index()`=1; `print()` shows "failsafe: YES" | D19F (second failover), D22T |
| DVG09 | g(5) crafted states: #0 OK, #1 NO_DATA+TIMEOUT, #2 NO_DATA+HIGH_ERRCOUNT+STALE, #3 NO_DATA+HIGH_ERRDENSITY, #4 unused; `print()` | output has "sensor #0 … OK", " OFF", " TOUT", " STALE", " ECNT", " EDNST", no "sensor #4", "failsafe: NO" | D22F D23 D24T/F D25–D30 T/F |
| DVG10 | `SqeDvgDeathTest`: `DataValidatorGroup g(0); g.add_new_validator();` | `EXPECT_DEATH(…, "")` — **characterization F-03** (null `_last`) | D05 path (crash) |
| DVG11 | g(2) both sensors silent after sensor 0 was best | idx −1, count 1, non-null return (F-02) | D14T (I∧J), D20T |
| DVG12 | construct/destroy g(0..5) with extra `add_new_validator()` calls | no crash (ASan run optional: no leak) | D04 |
Probe: `DISABLED_PRB03_AllFailedShouldReturnNull` (F-02, expects nullptr) — only if the humans approve the oracle (header: "pointer to the array of best values").
Optional (GCC only, separate binary `SqeDataValidatorAllocFaultTest.cpp`, see SPEC_02 §9): AF01 `add_new_validator()` returns nullptr on allocation
failure (covers D05T), AF02 ctor with failing first allocation leaves an empty group (documents F-13).

## Per-test structural evidence (O3 rows and viva)
`tools/sqe_coverage.sh pertest unit-SqeDataValidatorGroup 'SqeDvgMcdcTest.MC13_*' MC13` and the same for MC01 → compare hit counts of lines
207, 210, 213, 214 in `evidence/coverage/pertest/MC13_lines.txt` vs `MC01_lines.txt`. Then `tools/sqe_coverage.sh pertest-all unit-SqeDataValidatorGroup`
+ `python3 tools/sqe_unique_coverage.py evidence/coverage/pertest/unit-SqeDataValidatorGroup/` → "coverage lost if removed" table for P15.

## Done when
All MC/DC rows executed and matching expected outcomes; checker still passes; DataValidatorGroup.cpp 100 % lines, branches except
documented gaps (L88 alloc failure, L213 infeasible); shuffle ×5 stable. Gate G08.
## Explain-back
Draw the 12 D13 evaluations as a table and point to the one condition that differs in each pair; explain why MC04 counting as a failover is a
semantic question (F-11), not a test failure.
