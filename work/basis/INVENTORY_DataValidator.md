> **P05 verification note (2026-10-01):** every row below was re-checked line-by-line against the live source at
> `PX4-Autopilot/src/modules/sensors/data_validator/DataValidator.cpp` on commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`
> (`cat -n` read in full). All 15 decision IDs, line numbers, conditions, and outcomes match exactly — no corrections needed.
> Copied here (verbatim) from `docs/reference/REF_03_INVENTORY_DATAVALIDATOR.md` as the P05 working basis.

# REF_03 — Decision inventory: `src/modules/sensors/data_validator/DataValidator.cpp` (v1.17.0)
Functions: scalar `put()` L47–51 (no decisions; pads axes 1–2 with 0), `put()` L53–98, `confidence()` L100–142, `print()` L144–155.
Header facts: timeout 40000 µs, NORETURN_ERRCOUNT 10000, ERROR_DENSITY_WINDOW 100.0f, VALUE_EQUAL_COUNT_DEFAULT 100, dimensions 3.
State carried between calls: `_time_last`, `_event_count`, `_error_count`, `_error_density`, `_priority`, `_mean/_lp/_M2/_rms/_value[3]`,
`_value_equal_count` (**shared across axes**), `_error_mask` (accumulates with `|=` until a call returns > 0).

| ID | Line | Code | Type | Outcomes / notes | Tests |
|---|---|---|---|---|---|
| DV-D01 | 57 | `error_count_in > _error_count` | if | T: density += diff · F: go to D02 (also when count decreases) | T: DV08,13,14 · F: DV01,10 |
| DV-D02 | 60 | `_error_density > 0` | else-if (reached only if D01 F) | T: density−− · F: floor 0 | T: DV08,10 · F: DV01,09 |
| DV-D03 | 67 | `i < dimensions` | for | T ×3, F once per put | all puts |
| DV-D04 | 68 | `PX4_ISFINITE(val[i])` | if | F: axis ignored (no stats, no equality count, value kept) | T: DV01 · F: DV06,07 |
| DV-D05 | 69 | `_time_last == 0` | if (first sample) | T: init mean/lp/M2 · F: Welford update + equality check. Timestamp 0 re-enters init (F-12) | T: DV01,20 · F: DV02 |
| DV-D06 | 82 | `fabsf(_value[i]-val[i]) < 0.000001f` | if | T: ++count (per axis) · F: count = 0 | T: DV02(axes1,2),03,05 · F: DV02(axis0),04 |
| DV-D07 | 106 | `_time_last == 0` | if | T: NO_DATA, ret 0 | T: DV11,17,20 · F: DV01 |
| DV-D08 | 110 | `timestamp > _time_last + _timeout_interval` | else-if | T: TIMEOUT, ret 0 · boundary: equality is not a timeout | T: DV12,16,19 · F: DV12 |
| DV-D09 | 115 | `_value_equal_count > _value_equal_count_threshold` | else-if | T: STALE · boundary: count == threshold not stale | T: DV03,05 · F: DV03 |
| DV-D10 | 120 | `_error_count > NORETURN_ERRCOUNT` | else-if | T: HIGH_ERRCOUNT · boundary 10000 | T: DV13,17 · F: DV13 |
| DV-D11 | 125 | `_error_density > ERROR_DENSITY_WINDOW` | else-if | T: HIGH_ERRDENSITY + cap to 100 (ret stays 1 here, becomes 0 in D12 body) · density == 100 → F (F-10) | T: DV13,14 · F: DV14,15 |
| DV-D12 | 132 | `ret > 0.0f` | if | T: ret = 1 − density/100 · F: critical error | T: DV01,15 · F: DV11,13 |
| DV-D13 | 136 | `ret > 0.0f` | if (nested) | T: mask = NO_ERROR · F: mask kept (density ≥ 100) | T: DV01,04,12,14,16 · F: DV13,15 |
| DV-D14 | 146 | `_time_last == 0` (print) | if | T: "no data" + return | T: DV18 · F: DV19 |
| DV-D15 | 151 | `i < dimensions` (print) | for | 3 lines, each calls `confidence(hrt_absolute_time())` (side effect F-05) | DV19 |

Reachability notes: division by zero at L80 (`_event_count - 1`) is impossible — the update branch needs `_time_last != 0`, i.e. a previous put, so
`_event_count ≥ 2`. The NaN-first-sample path (DV07) skips initialisation for all axes (lp/mean start from 0 on the first finite sample).
Obligations: 15 decisions → 30 decision outcomes. Measured (v1.17.0 source compiled standalone, GCC 13.3, -O0 --coverage, PX4 math flags):
**58 executable lines, 30 gcov branch records** (exactly one pair per decision line: L57 L60 L67 L68 L69 L82 L106 L110 L115 L120 L125 L132 L136 L146 L151).
Re-read LF/BRF from your PX4 build (P03) — they should match or differ only slightly.
