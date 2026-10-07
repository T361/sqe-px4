# Revision v2 — what changed and what you must be able to explain in the viva

Read this together with the new test files. Every member should be able to answer every question below without notes.

## 1. Why the scope was extended
The course clarification said a narrow, convenient scope is not acceptable. We added `battery.cpp` (low-battery
warning → Commander failsafe) and the multicopter land detector (`landed` → auto-disarm). Scope is now 7 files,
897 executable lines, 204 decisions, 59 compound.

## 2. Test doubles (be ready to show each one)
| Double | File | One-sentence explanation |
|---|---|---|
| Test subclass / seam | `TestableBattery`, `TestableMcLandDetector` | Inherit from the production class and re-export protected methods with `using`; nothing is overridden, so the code under test is unchanged. |
| Link-time stub | `SqeFailureInjectorParamFaultTest.cpp` | Linked with `-Wl,--wrap=param_get`: every call to `param_get` goes to `__wrap_param_get`, which fails only for `SYS_FAILURE_EN` and otherwise calls `__real_param_get`. |
| Link-time fake | `SqeFailureDetectorCopyFaultTest.cpp`, `SqeBatteryTest.cpp` | `--wrap` on the mangled name of `uORB::Manager::orb_data_copy`; one-shot failure for one topic, so `copy()` fails right after `updated()` returned true. |
| Allocator fake | `SqeDataValidatorGroupAllocTest.cpp` | replacement global `operator new` returning nullptr once (NuttX behaviour). |
| Real work queue | `SqeLandDetectorRunTest.cpp` | `Run()` is private and comes through a *private* base class, so we start the real work-queue manager and the detector's own `start()`, then observe `vehicle_land_detected`. `SetUpTestSuite` calls `hrt_work_queue_init(); hrt_init(); WorkQueueManagerStart();` — the part of `px4::init_once()` the gtest runner skips. |

Likely question: *"Isn't wrapping uORB cheating?"* — No production code changes; the fake forwards every call except
one armed failure; it only exists in two dedicated binaries; it reproduces a real failure mode (copy failing) that
cannot be produced single-threaded.

## 3. Gaps that remain (know the proof)
- G-02 DVG:78 — compiler's null check in `delete p` (show `evidence/v2/gaps/G-02_delete_null_check.txt`).
- G-03 DVG:213 — `best` is always non-null when D15 is true (same block assigns both).
- G-09 battery:130 op4 — `_internal_resistance_initialized` is only set true when `n_cells > 0`, and reset whenever
  `n_cells` changes, so op4 can't be false when op3 is true.
- G-10 LandDetector:187 op3 — take-off time is only set while armed and reset on the first disarmed cycle.
- 244 exception edges — compiler-generated; NuttX builds use `-fno-exceptions`.

## 4. MC/DC — practise these pairs
- DVG-D13 E pair: MC-01 vs MC-03 (confidence 0.95 vs 0.93, |Δ| 0 vs 0.02 against 0.01).
- MLD-D35 (maybe landed, 7 conditions): G pair MC-13 (8 s not elapsed) vs MC-14 (held 8.1 s); E pair MC-09 vs MC-13
  (vertical estimate valid vs invalid; F evaluated only in MC-09, G only in MC-13, equal logical values → unique-cause).
- MLD-D37 (ground effect): S and U read the same `takeoff_state`, so they can never both be true; S pair MC-18/MC-19,
  U pair MC-16/MC-20.

## 5. "What coverage is lost if this test is removed?"
- SQE-FIP-01 → FailureInjector.cpp:43 `param_get != OK` branch (G-06 reopens).
- SQE-FDC-01 → FailureDetector.cpp:194 copy-false branch (G-07 reopens).
- SQE-LDB-01 → LandDetector.cpp:83 `timestamp == 0` operand.
- SQE-BAT-26 → battery.cpp:356 copy-false branch.
- SQE-MLD-MC-14 → MulticopterLandDetector.cpp:290 `_minimum_thrust_8s_hysteresis` True outcome.

## 6. Findings changed
F-09/F-10/F-02 are now *observations* (their probes asserted an oracle we invented; the assignment forbids manufacturing
failures). F-07a/F-07b remain real defects, confirmed again with ASan/UBSan.

## 7. Mutation analysis in one sentence
We changed one operator at a time in the production code and checked the tests fail: 85 % (old files) and 83 % (new
files) of valid mutants are killed, every logical-operator mutant of the MC/DC decisions is killed, and each survivor is
explained (equivalent, impossible float value, microsecond-exact timing).
