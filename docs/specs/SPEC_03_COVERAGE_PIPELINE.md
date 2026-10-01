# SPEC_03 — Coverage pipeline (binding)

## 1. Instrumentation (what PX4 already does — do not change)
`PX4_CMAKE_BUILD_TYPE=Coverage` → `cmake/coverage.cmake`: all targets get `--coverage -fprofile-update=atomic`; GCC C++ flags
`--coverage -ftest-coverage -fprofile-arcs -O0 -fno-default-inline -fno-inline -fno-elide-constructors`. Objects write `.gcno` at compile time and
`.gcda` next to the object at process exit (counts merge across processes/binaries that link the same object).
Production objects are compiled **once** (static libraries `data_validator`, `failure_detector`) and linked into `px4` and into our test binaries,
so baseline (upstream) and student runs update the same `.gcda` → zero counters between captures.

## 2. The script `tools/sqe_coverage.sh`
| Mode | Does |
|---|---|
| `doctor` | print lcov/genhtml/gcov versions, detected rc keys, gcov tool, `CMAKE_BUILD_TYPE` from the cache |
| `build` | `make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__ [-j$JOBS]` → `evidence/build/coverage_build.log` |
| `baseline [label]` | zero → `ctest -E 'Sqe'` (upstream only) → capture → `evidence/coverage/<label or baseline>/` |
| `student [label]` | zero → `ctest -R 'Sqe'` → capture → `evidence/coverage/<label or student>/` |
| `final [label]` | zero → full ctest → capture → `evidence/coverage/<label or final>/` |
| `pertest <binary> <gtest_filter> <label>` | zero (scope objdirs) → run one binary with the filter → capture scope → `evidence/coverage/pertest/<label>.*` |
| `pertest-all <binary>` | `pertest` for every test listed by `--gtest_list_tests` → `evidence/coverage/pertest/<binary>/` |
Environment overrides: `PX4_DIR`, `BUILD_DIR`, `GCOV_TOOL` (macOS: `tools/llvm-gcov.sh`), `JOBS`, `SQE_CAPTURE=scope|all`,
`SQE_LCOV_EXTRA_IGNORE` (comma list for lcov ≥ 2 only), `SQE_LCOV_FILTER` (e.g. `branch`, lcov ≥ 2 only — changes denominators; report it).

## 3. Capture recipe (what the script runs)
```bash
lcov --capture --initial --directory <objdirs> --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" $RC $IGN -o initial.info   # zero records
lcov --capture           --directory <objdirs> --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" $RC $IGN -o run.info
lcov -a initial.info -a run.info $RC $IGN -o all.info
lcov --extract all.info '*/sensors/data_validator/DataValidator.cpp' '*/sensors/data_validator/DataValidatorGroup.cpp' \
     '*/failure_detector/FailureDetector.cpp' '*/failure_detector/FailureInjector.cpp' $RC $IGN -o scope.info
genhtml scope.info --branch-coverage --legend --prefix "$PX4_DIR" --title "SQE A2 <label>" -o html $GENHTML_RC
lcov --summary scope.info $RC ; python3 tools/sqe_lcov_report.py summary scope.info > per_file.md
```
`<objdirs>` = `build/px4_sitl_test/src/modules/sensors/data_validator` and `build/px4_sitl_test/src/modules/commander/failure_detector`
(fast; contains the production objects and our test objects). `SQE_CAPTURE=all` captures the whole build dir (slow; optional context).

## 4. lcov version handling (auto-detected; recorded in MANIFEST) — **tested with lcov 2.0-1 (Ubuntu 24.04) + gcov 13.3**
| lcov | branch key (detected from installed lcovrc) | exception filter | ignore |
|---|---|---|---|
| 1.x (Ubuntu 22.04: 1.15) | `lcov_branch_coverage=1` | opt-in `geninfo_no_exception_branch=1` | none (1.x rejects `mismatch`) |
| 2.0 (Ubuntu 24.04) | `branch_coverage=1` (its `/etc/lcovrc` has `branch_coverage` and `no_exception_branch`; the 1.x name still works as alias) | **leave off** | `--ignore-errors mismatch` |
| ≥ 2.1 (Homebrew) | `branch_coverage=1` | leave off unless verified | `--ignore-errors mismatch`; ≥ 2.3 also knows `mcdc_coverage` |
**Verified trap:** with lcov 2.0 + gcov 13, adding `--rc no_exception_branch=1` removed **all** branch records (BRDA 430 → 0).
The script therefore enables exception filtering only with `SQE_LCOV_NO_EXCEPTION=1`, re-captures without it if branch data vanishes (noted in
the MANIFEST), and aborts when `scope.info` has no branch data at all. gcov 13 printed no `(throw)` branches for these files in testing, so the
filter is normally unnecessary; any exception edge that does appear is classified as a tool artefact in GAPS.
**Do not set `geninfo_unexecuted_blocks=1`**: lcov 2.x warns "unexecuted block on non-branch line" for lines such as DataValidatorGroup L55
(the `-fcheck-new` null path is an unexecuted block on an executed line); that option would *zero* those lines and create false gaps.
If lcov stops with an error category (e.g. `negative`, `unused`, `inconsistent`), add it via `SQE_LCOV_EXTRA_IGNORE` **and** write it in the MANIFEST
notes; never ignore errors silently.
Test result of the pipeline on real gcov data (standalone build of the v1.17.0 data_validator sources): lcov figures identical to `gcov -b`
(DataValidator.cpp 30 branches, DataValidatorGroup.cpp 116 branches).

## 5. Interpreting the numbers (put this in the report once)
- **Lines** (`LF/LH`) ≈ executable statements. **Functions** (`FNF/FNH`).
- **Branches** (`BRF/BRH`, genhtml `[+ -]` markers) are gcov *object-code* edges: every short-circuit operand of `&&`/`||` produces its own
  True/False edge pair, loops produce entry/exit edges, `switch` produces one edge per case plus default, ternaries a pair. Hence 100 % gcov branch
  coverage implies every evaluated atomic condition took both values (short-circuit condition coverage) — stronger than decision coverage.
- gcov attributes each edge to the line where GCC emits the jump, not necessarily the operand's source line (measured, GCC 13.3 -O0 on the
  v1.17.0 sources: DataValidatorGroup L187–L189 = 16 branch records for the 7-condition decision; L203 = 6; L207+L208 = 6; the six print
  ternaries = 12 at L260; file total 116; DataValidator.cpp total 30). Float compares may add an edge.
- Compiler-generated paths: the `-fcheck-new` null check after `new` (L55, L86) showed up as an *unexecuted block* (gcov marks the line
  `40*`), not as branch records; exception edges on calls (filtered where the lcov version supports it); destructor/cleanup edges.
  These are **tool artefacts** unless a source-level outcome corresponds; list each that appears in GAPS.
- "Not executed" ≠ "False": an inner decision that was never reached contributes nothing (lecture 6).
- Report raw tool percentages and, separately, feasible coverage (excluding items with a written infeasibility proof) — never merge them.

## 6. Outputs per capture directory
`MANIFEST.md` (UTC time, git HEAD + branch, dirty flag, tool versions, detected flags, objdirs, ctest regex, ctest exit code, number of tests),
`ctest.log`, `initial.info`, `run.info`, `all.info`, `scope.info`, `summary.txt`, `per_file.md`, `per_file.json`, `html/`.
Per-test: `<label>.info`, `<label>_lines.txt` (hit count per line of scope files), `<label>.log`.

## 7. Rules
Never capture while another test process runs · zero before every capture · never mix `.info` from different builds · never edit `.info` by hand ·
keep baseline evidence from P03 untouched (read-only after G03) · recapture after any rebuild of production objects.
