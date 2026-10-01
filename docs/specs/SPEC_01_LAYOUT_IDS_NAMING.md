# SPEC_01 — Layout, identifiers and naming (binding)

## 1. Workspace layout
See CLAUDE.md §2. `PX4-Autopilot/` is the only git repository whose diff is submitted; `work/` and `evidence/` stay outside it.

## 2. Test source files (inside PX4-Autopilot)
| File | Registered as (ctest) | Level |
|---|---|---|
| `src/modules/sensors/data_validator/SqeDataValidatorTest.cpp` | `unit-SqeDataValidator` | unit |
| `src/modules/sensors/data_validator/SqeDataValidatorGroupTest.cpp` | `unit-SqeDataValidatorGroup` | unit |
| `src/modules/sensors/data_validator/SqeDataValidatorAllocFaultTest.cpp` (optional, GCC) | `unit-SqeDataValidatorAllocFault` | unit |
| `src/modules/commander/failure_detector/SqeFailureDetectorTest.cpp` | `functional-SqeFailureDetector` | functional |
| `src/modules/commander/failure_detector/SqeFailureDetectorImuTest.cpp` | `functional-SqeFailureDetectorImu` | functional |
| `src/modules/commander/failure_detector/SqeFailureInjectorTest.cpp` | `functional-SqeFailureInjector` | functional |
Name derivation (cmake/px4_add_gtest.cmake): basename without extension, **every** "Test" substring removed, prefixed `unit-`/`functional-`.
Never put "Test" inside the meaningful part of the name (e.g. `SqeTestX` would become `SqeX`). Names must be unique across the repo.

## 3. Test IDs and gtest names
| ID pattern | gtest name prefix | Fixture(s) | Meaning |
|---|---|---|---|
| `SQE-DV-nn` | `DVnn_` | `SqeDataValidatorTest` | DataValidator structural |
| `SQE-DVG-nn` | `DVGnn_` | `SqeDvgTest`, `SqeDvgDeathTest` | DataValidatorGroup structural |
| `SQE-DVG-MC-nn` | `MCnn_` | `SqeDvgMcdcTest` | MC/DC scenarios |
| `SQE-DVG-AF-nn` | `AFnn_` | `SqeDvgAllocFaultTest` | optional allocation-fault tests |
| `SQE-FD-nn` | `FDnn_` | `SqeFailureDetectorTest` | FailureDetector |
| `SQE-FDI-nn` | `FDInn_` | `SqeFailureDetectorImuTest` | imbalanced-prop (multi-instance IMU) |
| `SQE-FI-nn` | `FInn_` | `SqeFailureInjectorTest` | FailureInjector |
| `SQE-PRB-nn` | `DISABLED_PRBnn_` | any | defect probes (expected FAIL, run explicitly) |
Example: `TEST_F(SqeDvgMcdcTest, MC04_EqualPrioHigherConf_SwitchCountsAsFailover)` ↔ `SQE-DVG-MC-04`.
Mapping regex used by the tools: `^(DISABLED_)?(DVG|DV|MC|AF|FDI|FD|FI|PRB)(\d{2})_` (order matters: DVG before DV, FDI before FD).
IDs are never reused; a deleted test leaves a gap and a DECISIONS entry.

## 4. Decision, condition, finding, gap IDs
- Decisions: `DV-Dnn`, `DVG-Dnn`, `FD-Dnn`, `FI-Dnn` (numbered by source line order; REF_03–REF_05 are authoritative).
- MC/DC conditions: letters per decision as in REF_06 (DVG-D13: A–G; D14: H–J; D15: K–M; D32/D34: P–R).
- Findings `F-nn` (REF_07), investigations `INV-nn`, gaps `G-nn`, decisions log `D-nnn`, observations `O-nn`, iterations `IT-n`, audits `Gxx`.

## 5. Submission names
Base name `BASE=<Roll1_Roll2_Roll3_Section>` from `work/TEAM.md` line 1 (e.g. `22I-0001_22I-0002_22I-0003_SE-A`). Files:
`<BASE>.pdf` (report), `<BASE>.xlsx` (workbook), `<BASE>.patch` (git diff), `<BASE>.zip` (tests, coverage, logs, scripts, README).
If the LMS prescribes a different convention, it wins — record it in DECISIONS.
