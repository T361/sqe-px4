# REF_04 — Decision inventory: `src/modules/sensors/data_validator/DataValidatorGroup.cpp` (v1.17.0)
State: `_first`, `_last` (sibling list), `_timeout_interval_us`, `_curr_best` (−1), `_prev_best` (−1), `_first_failover_time` (0, no getter),
`_toggle_count` (0; `failover_count()`), `MIN_REGULAR_CONFIDENCE` 0.9f. Header contract: `add_new_validator()` "returns … nullptr on error";
`get_best()` "@return pointer to the array of best values".

| ID | Line | Code | Type | Outcomes / notes | Tests |
|---|---|---|---|---|---|
| DVG-D01 | 54 | `i < siblings` | for (ctor) | 0 iterations for g(0) | DVG01,02,03 |
| DVG-D02 | 57 | `i == 0` | if | T: `_first` · F: link sibling | T: DVG01 · F: DVG02 |
| DVG-D03 | 69 | `_first` | if | F only for g(0) | T: DVG01 · F: DVG03 |
| DVG-D04 | 76 | `_first` | while (dtor) | T/F | DVG12 |
| DVG-D05 | 88 | `!validator` | if | T needs `new` → nullptr (NuttX semantics); POSIX throws → env-limited (AF01) | F: DVG04 · T: AF01/gap |
| — | 55, 86 | `new DataValidator()` | GCC `-fcheck-new` null check | measured: no branch record, but an *unexecuted block* (gcov shows `40*`); never set `geninfo_unexecuted_blocks=1` | tool artefact / AF |
| DVG-D06 | 103 | `next != nullptr` | while (set_timeout) | | DVG03,04,05 |
| DVG-D07 | 116 | `next != nullptr` | while (threshold) | | DVG03,06 |
| DVG-D08 | 129 | `next != nullptr` | while (put) | exit without break for out-of-range index | DVG02 |
| DVG-D09 | 130 | `i == index` | if | T: put + break · F: next | DVG02 |
| DVG-D10 | 157 | `next != nullptr` | while (loop 1) | exits by F only when `_curr_best == -1` | MC11–13 (F), others (break) |
| DVG-D11 | 158 | `i == pre_check_best` | if | T: seed best + break | T: MC01… · F: MC11–13 |
| DVG-D12 | 179 | `next != nullptr` | while (loop 2) | | all get_best |
| **DVG-D13** | 187–190 | `((A&&B)||(C&&D)||(E&&F))&&G` | compound, 7 conds, per sibling | MC/DC — REF_06 §3 | MC01–MC12, DVG07 |
| **DVG-D14** | 203 | `H || (I && J)` | compound, 3 conds | MC/DC — REF_06 §4 | MC01,02,11,14,18,19 |
| **DVG-D15** | 207–208 | `K && L && M` | compound, 3 conds | MC/DC — REF_06 §5 | MC01,04,13,20 |
| DVG-D16 | 213 | `best != nullptr` | if | F **infeasible**: D15 T ⇒ K T ⇒ seeded in L162–168 (G-03) | T: MC01 |
| DVG-D17 | 219 | `_curr_best < 0` | if | T: initial bookkeeping (no toggle) · F: real switch | T: MC12,13 · F: MC04,14 |
| DVG-D18 | 226 | `true_failsafe` | if | T: toggle++ · F: priority switch | T: MC04,14 · F: MC01 |
| DVG-D19 | 230 | `_first_failover_time == 0` | if | effect unobservable (no getter, F-14) | T: MC14 · F: DVG08 |
| DVG-D20 | 234 | `max_confidence < FLT_EPSILON` | if | T: index −1 (only sensor failed) · F | T: MC18,DVG11 · F: MC14 |
| DVG-D21 | 245 | `(best) ? value : nullptr` | ternary | T: non-null (also after total failure, F-02) · F: nothing ever selected | T: MC01 · F: MC11,19 |
| DVG-D22 | 251 | `_toggle_count > 0` | ternary (print) | "YES"/"NO" | DVG08, DVG09 |
| DVG-D23 | 256 | `next != nullptr` | while (print) | | DVG09 |
| DVG-D24 | 257 | `next->used()` | if | unused validators skipped | DVG09 |
| DVG-D25…D30 | 261–266 | flag ternaries OFF/STALE/TOUT/ECNT/EDNST/OK | 6 ternaries | each T and F across validators | DVG09 |
| DVG-D31 | 281 | `next != nullptr` | while (failover_index) | | MC14–17 |
| **DVG-D32** | 282–283 | `used() && state()!=NO_ERROR && i==(unsigned)_prev_best` | compound, 3 conds | MC/DC — REF_06 §6; `(unsigned)-1` never equals i | MC14–17 |
| DVG-D33 | 300 | `next != nullptr` | while (failover_state) | | MC14–17 |
| **DVG-D34** | 301–302 | same as D32 | compound, 3 conds | returns flags | MC14–17 |
| DVG-D35 | 318 | `next != nullptr` | while (get_sensor_state) | UINT32_MAX when not found | DVG01 |
| DVG-D36 | 319 | `i == index` | if | | DVG01 |
| DVG-D37 | 336 | `next != nullptr` | while (get_sensor_priority) | 0 when not found | DVG01 |
| DVG-D38 | 337 | `i == index` | if | | DVG01 |
Obligations: 38 decision IDs (5 compound for MC/DC). Measured (standalone GCC 13.3 -O0 --coverage of the v1.17.0 source): **155 executable lines,
116 gcov branch records**; D13 = 16 records attributed to L187 (4), L188 (2), L189 (10); D14 = 6 (L203); D15 = 6 (L207 4, L208 2); print ternaries = 12 (L260);
L213 branch 1 (`best == nullptr`) never taken — consistent with G-03. Re-read LF/BRF from your PX4 build.
