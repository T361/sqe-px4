# Remaining coverage gaps (format SPEC_10 §4) — every uncovered item of evidence/coverage/final/scope.info must appear here

Status: produced during P10 coverage iteration (IT-1, IT-2). Source captures: `evidence/coverage/IT-1/scope.info`,
`evidence/coverage/IT-2/scope.info`, cross-checked against `evidence/coverage/final/scope.info` (final capture, after
all P10 test additions). Raw gcov branch indices quoted below were re-derived directly from `gcov -b -c` output on
the live build (not merely the lcov BRDA summary) wherever a tool-artifact classification is claimed — see the
per-item "Why" for the exact reproduction command / evidence path.

Summary of outcome: of the 57 items open after IT-1, **5 genuine decisions (10 branch outcomes across FD-D25,
FD-D38, FD-D41) were closed by 3 new tests** (SQE-FD-36, SQE-FD-37, SQE-FDI-08) in IT-2. **Update 2026-10-05 (IT-4):**
2 more branch outcomes (`DataValidatorGroup.cpp` L86, L88) plus one previously-"line not executed" statement (L89)
were closed by implementing the allocation-fault-injection technique documented but previously unused in G-01 — see
G-01 below for the full account, including the one remaining line (L55) that stays uncovered for a narrower,
coverage-tooling-specific reason (death-test processes never flush gcov counters), not a reachability gap. With
this change, `DataValidatorGroup.cpp` reaches 100.0% line coverage and total scope line coverage reaches
**100.0%** (428/428) — see `evidence/coverage/IT-4/per_file.md`. Every remaining item is one of: environment-limited
(G-01's one remaining line, now narrower in scope), infeasible (2 clusters, proofs below), or tool-artefact (3
clusters, proofs below). No item was excluded merely to raise the percentage; no `LCOV_EXCL_*` marker was added to
production code.

---

### G-01 DataValidatorGroup.cpp:88 `if (!validator)` True — and the paired `-fcheck-new` null-check edges at
L55 branch 1 (`next = new DataValidator()`) and L86 branch 2 (`DataValidator *validator = new DataValidator()`),
plus L89 (`return nullptr;`, "line not executed")
Class: **partially closed** — `add_new_validator()`'s half (L86, L88, L89) is now covered by a real test; the
constructor's half (L55) remains environment-limited for a gcov-specific reason, not a reachability reason.

**UPDATE (2026-10-05, IT-4):** the allocation-fault injection technique previously only documented in
`docs/specs/SPEC_02_TEST_CODE_STANDARD.md` §9 as "not implemented this session" has now been implemented —
`PX4-Autopilot/src/modules/sensors/data_validator/SqeDataValidatorAllocFaultTest.cpp` (ctest
`unit-SqeDataValidatorAllocFault`, a separate GCC-only binary since it overrides global `operator new`/`delete`
and cannot share a process with any other test). This was done specifically because the course clarified that
documented mock/fault-injection techniques must be attempted before a gap is called infeasible, not skipped by
default.

- **`add_new_validator()`'s branch (L86, L88, L89) — now genuinely covered**, confirmed by re-reading the fresh
  `evidence/coverage/IT-4/scope.info` directly: `BRDA:86,0,1,9` (9 hits, was 0), `BRDA:88,0,1,10` (10 hits, was 0),
  `DA:89,9` (9 hits, was 0 — "line not executed" is no longer true). Test `SQE-DVG-AF-01` arms the *next*
  allocation to fail via the override, calls `add_new_validator()` on a healthy group, and asserts it returns
  `nullptr` with the group otherwise unchanged — exactly the documented `@return ... nullptr on error` contract.
  `DataValidatorGroup.cpp`'s own line coverage is now **100.0%** (155/155, up from 154/155) and branch coverage
  **96.6%** (112/116, up from 110/116) — see `evidence/coverage/IT-4/per_file.md`.
- **Constructor's branch (L55) — still 0 hits, but now for a different, narrower reason.** Test `SQE-DVG-AF-02`
  arms the constructor's *first* allocation to fail; tracing the real control flow (not assumed — walked line by
  line against `DataValidatorGroup.cpp:48-65`), this does NOT crash at `_first->get_timeout()` (correctly guarded
  by `if (_first) {...}` at L69) — it crashes one iteration later at `prev->setSibling(next)` (L61), where `prev`
  is still null because the first (failed) allocation never updated it to a real pointer. This is a genuine,
  confirmed `SIGSEGV`, asserted correctly as a death test (`EXPECT_DEATH`). **Why L55 still shows 0 hits despite
  the test exercising exactly that line:** gcov/gcov_dump data is written by each process's `atexit` handler;
  `EXPECT_DEATH`'s forked child process is killed by the SIGSEGV before that handler runs, so the coverage counters
  incremented during the crashing child's brief execution are never flushed to the `.gcda` file. This is a known,
  general limitation of statement-coverage instrumentation on any test that intentionally crashes (the same
  reason F-03/DVG-10's existing, pre-this-session death test also does not move any coverage number for its own
  target line) — not a sign the test is fake or the branch is unreached; the test's PASS result and the actual
  process-level SIGSEGV are independently verifiable (`evidence/tests/xml/unit-SqeDataValidatorAllocFault.xml`,
  `evidence/tests/logs/unit-SqeDataValidatorAllocFault_shuffle.log`, both captured from live runs).
Counted in feasible coverage? **L86/L88/L89: yes, now genuinely covered, not merely "feasible."** L55: no — this
one line remains a tool-measurement limitation of coverage-vs-death-tests specifically, not an unattempted or
unreachable branch.

### G-02 DataValidatorGroup.cpp:78 `delete (_first);` branch 1 (0 hits)
Class: tool-artefact
What exactly is uncovered: gcov raw branch 1 at L78 (`branch 1 taken 0`), re-derived via `gcov -b -c -o
<objdir> DataValidatorGroup.cpp.gcno` on a fresh full-suite run (`ctest -R Sqe`), reproduced in
`/tmp/.../scratchpad/gcov_check/DataValidatorGroup.cpp.gcov` this session.
Why: this is the compiler-generated null-pointer guard that `delete` emits before invoking the destructor /
deallocation path (the C++ ABI's `delete` on a `T*` checks for null before calling `~T()`, even though the
expression is statically guarded here: `while (_first) { ...; delete(_first); ...}` — `_first` is checked
non-null by the enclosing `while` condition on every iteration before the `delete` is reached, so the null branch
of `delete`'s own internal check has no reachable source-level counterpart; it is an artefact of how `delete`
lowers to IR, not a condition the source code expresses.
What would reach it: none — `delete` on a guaranteed-non-null pointer; the null-check edge is only reachable by
calling `delete` with the loop invariant broken, which is impossible given the loop guard directly dominates it.
Counted in feasible coverage? no

### G-03 DataValidatorGroup.cpp:213 `best != nullptr` False
Class: infeasible
What exactly is uncovered: L213 lcov `BRDA:213,0,1` (0 hits) — the False side of `if (best != nullptr)`.
Why (full proof, re-derived independently this session, matches `work/basis/CFG_DataValidatorGroup_get_best.md`'s
pre-existing DVG-D16 analysis): this `if` is only reached when DVG-D15 (`pre_check_prio != -1 && pre_check_prio <
max_priority && |conf diff| < 0.1`) evaluates True, i.e. its first operand K (`pre_check_prio != -1`) is True.
`pre_check_prio` is initialised to `-1` (L148) and is only ever reassigned inside Loop 1's `if (i ==
pre_check_best)` body (L158-169), in the exact same `if` block that also assigns `best = next` (L168) — both
assignments are unconditional once that block is entered, with no intervening branch or early return between
L162 (`pre_check_prio = prio;`) and L168 (`best = next;`). Therefore: K True ⇒ Loop 1's seed block executed ⇒
`best` was assigned a non-null pointer (`next`, which is non-null because it came from `while (next != nullptr)`
at L157) ⇒ `best != nullptr` is unconditionally True whenever DVG-D15 (and hence this `if`) is reached at all.
There is no path that sets `pre_check_prio != -1` without also setting `best`.
What would reach it: none within this source — would require decoupling the two assignments (a production code
change, out of scope for R2), or `best` being externally mutated to null between L168 and L213 (impossible, no
other write to `best` in the function except L168 and the Loop 2 update at L191-195, which also always assigns a
non-null `next`).
Counted in feasible coverage? no

### G-04 DataValidatorGroup.cpp:260 — all 6 ternary branch-pairs in `print()`'s `PX4_INFO_RAW(...)` call
(L261 NO_DATA, L262 STALE_DATA, L263 TIMEOUT, L264 HIGH_ERRCOUNT, L265 HIGH_ERRDENSITY, L266 NO_ERROR/"OK";
gcov/lcov branch indices b0.0..b0.11 on L260, since all 6 pairs are mis-attributed to the call's first line)
Class: tool-artefact
What exactly is uncovered: lcov reports `BRDA:260,0,10,0` (the "OK" ternary's true/fallthrough side) with 0 hits,
and a scrambled, internally-inconsistent distribution across the other 5 pairs (e.g. `BRDA:260,0,6,0` for what
should be the TOUT ternary, also 0, even though TOUT was never the triggered flag in any test — consistent with
the attribution error, not with the real condition never being true).
Why: standalone, independently reproduced minimal repro this session
(`/tmp/.../scratchpad/t2.cpp`, 6-ternary `printf` call spanning multiple source lines, built with the exact
project coverage flags `--coverage -ftest-coverage -fprofile-arcs -O0 -fno-default-inline -fno-inline
-fno-elide-constructors`, GCC 13.3.0): **all 6 ternary conditions' branch instrumentation collapses onto the call's
opening line** in the `.gcov` output; the ternaries' own source lines show no branch records at all. Cross-checked
against the real captured output of `SqeDvgTest.DVG09_PrintSkipsUnusedSensorsAndShowsPerSensorFlags` via a
temporary `fprintf(stderr, ...)` debug line added to (and immediately reverted from) the test file this session:
sensor #0 genuinely prints `" OK"` (flags==0, the ternary's True side genuinely executes), yet gcov's branch 10 at
L260 reports 0 hits regardless — proving the instrumentation, not the test, is wrong. This is consistent with a
known class of gcov/GCC limitation where DWARF line-table discriminators for several structurally-identical
conditional-move-style expressions passed as consecutive variadic-call arguments are collapsed/merged at `-O0`.
This file:line was independently flagged in `work/basis/INVENTORY_DataValidatorGroup.md` (L23) as "measured: no
branch record, but an unexecuted block marker" for the sibling `-fcheck-new` pattern — this is the same family of
measurement artefact, now additionally confirmed by direct reproduction for the ternary-attribution case.
What would reach it: none via gcov/lcov on this toolchain; a single-line reformatting of the call (one ternary per
`printf`/temporary variable, i.e. restructuring `print()`) would fix the *measurement*, but that is a production
code change (out of R2 scope) and was not made. A newer GCC/gcov (the project is pinned to GCC 13.3.0 per P01) or
switching to `llvm-cov`/Clang's source-based coverage (not integrated into this project's toolchain, see
`tools/llvm-gcov.sh`'s very narrow purpose) might avoid the collapse, untested this session.
Counted in feasible coverage? no (confirmed measurement artefact, not a real code-coverage gap: the semantic test
coverage already exists via SQE-DVG-09/SQE-DVG-08's assertions on captured stdout text)

### G-05 Exception / compiler-generated unwind edges (all `be0.x` / `e0.x` branch indices across
FailureDetector.cpp and FailureInjector.cpp)
Class: tool-artefact
What exactly is uncovered: every lcov `BRDA:<line>,e0,<n>,0` entry in both files — e.g. FailureDetector.cpp L46
(19 pairs, the `ModuleParams(parent)` constructor's 19 `Param<>` member initialisers), L52/L57/L60/L73/L74/L77/
L81/L86/L96/L98/L99/L100/L111/L112/L113/L114/L126/L129-132/L144/L149/L152/L153/L161/L174/L175/L183/L191/L194/
L199/L203/L208/L229/L237/L260/L264, and FailureInjector.cpp L43/L55/L75/L83/L89/L95/L107/L108 (full location list
in `evidence/coverage/final/scope.info`, searchable by the `e0,` branch-type prefix).
Why: directly confirmed via `gcov -b -c` raw output this session (`/tmp/.../scratchpad/gcov_check/*.gcov`): every
one of these entries is annotated `branch N taken 0 (throw)` — GCC's `--coverage` instrumentation inserts an
exception-unwind edge after essentially every function call and non-trivial statement when C++ exceptions are
enabled (no `-fno-exceptions` in `CMAKE_CXX_FLAGS_COVERAGE`), representing "a C++ exception propagated out of this
call" — a path that requires something in the call graph (uORB internals, `matrix::` library, `hrt_absolute_time`,
`Hysteresis`, `AlphaFilter`, `Param<>` constructors, `PX4_INFO`/`PX4_INFO_RAW`/`printf`) to actually `throw`.
None of these PX4 library functions throw by design (PX4 flight code is written to the "never throw" convention
even though exceptions are technically enabled in this host/SITL test build) — this matches CLAUDE.md §8 pitfall 6
("gcov counts one branch pair per short-circuit operand … tool 'branch coverage' ≈ condition coverage") and the
exception-edge variant of the same measurement phenomenon.
What would reach it: making one of these library calls genuinely throw (e.g. `std::bad_alloc` from an allocation
failure deep in a uORB/matrix call, or a fault-injected exception) — not a realistic or intended behaviour for
flight-code call sites; `SQE_LCOV_NO_EXCEPTION=1` (the project's opt-in `--rc no_exception_branch=1` lcov flag,
see `tools/sqe_coverage.sh`'s `setup_flags()`) is available precisely to filter this category, but was **not**
turned on this session's captures (kept the raw tool number for the report per SPEC_03/P10's "report raw, then
feasible" rule) — re-running with that flag set would make the raw and feasible branch percentages converge for
this file pair; left as a reporting-time choice rather than silently suppressed.
Counted in feasible coverage? no

### G-06 FailureInjector.cpp:43 first operand False (`param_get(param_find("SYS_FAILURE_EN"), &param_sys_failure_en)
!= PX4_OK`, lcov `BRDA:44,0,1,0`)
Class: infeasible
What exactly is uncovered: the branch where `param_get()` returns something other than `PX4_OK` immediately after
`param_find("SYS_FAILURE_EN")`.
Why: `SYS_FAILURE_EN` is a statically-registered PX4 parameter, defined via `PARAM_DEFINE_INT32` in
`PX4-Autopilot/src/lib/systemlib/system_params.c` (confirmed by `grep -rln SYS_FAILURE_EN PX4-Autopilot/src/`,
line 303 per `work/basis/INVENTORY_FailureDetector_Injector.md`'s header note) — this file is compiled into the
PX4 parameter table at build time via the `px4_add_module`/parameter-metadata generation step, so `param_find()`
on this exact, hard-coded, compile-time-constant string **cannot** return an invalid handle in a correctly-built
binary (its only failure mode, `PARAM_INVALID`, is for *unregistered* names — a typo or a parameter removed from
the build, neither of which applies here since the name is spelled identically in both the registration site and
the call site and the build compiled/linked successfully, proving the parameter exists in the table). Given a
valid, non-`PARAM_INVALID` handle, `param_get()` only fails for a null handle or (per
`PX4-Autopilot/src/lib/parameters/param.cpp`'s contract) other handle-validity issues — not applicable here.
What would reach it: deliberately corrupting the parameter table at runtime (not available via the public test
API), or building with the parameter definition removed (a production code change, out of R2 scope and would
break the module's own build). Not pursued.
Counted in feasible coverage? no

### G-07 FailureDetector.cpp:194 and :222 `copy()` False immediately after `updated()` True
(FD-D21 at L194, FD-D27 at L222 — `work/basis/INVENTORY_FailureDetector_Injector.md`'s pre-flagged
"investigate" rows)
Class: infeasible (single-threaded test environment)
What exactly is uncovered: L194 `if (_sensor_selection_sub.copy(&selection))` False, and L222
`if (_vehicle_imu_status_sub.copy(&imu_status))` False — both guarded by a preceding `.updated()` check (L191,
L220) that was True.
Why (traced this session through the actual uORB implementation, not merely asserted): `Subscription::updated()`
(`platforms/common/uORB/Subscription.hpp:131`) delegates to `Manager::updates_available(_node, _last_generation)`,
which reads `DeviceNode::_generation.load() - generation` (`uORBDeviceNode.hpp:198`). `_generation` is only ever
incremented inside `DeviceNode::write()` (a successful publish), which is also the **only** place `_data` is
allocated (`uORBDeviceNode.cpp` L140-176, `if (nullptr == _data) { ...; _data = px4_cache_aligned_alloc(...); }`
before the first write completes). Therefore `updates_available() > 0` (i.e. `.updated()==true`) implies at least
one successful `write()` has already happened, which implies `_data != nullptr`. `Subscription::copy()`
(`Subscription.hpp:157`) calls `Manager::orb_data_copy()`, which only fails when `!is_advertised(node)` (false
here, since a write already happened, so the node was already marked advertised — advertising is a prerequisite
for `write()`) or (for queued topics) `only_if_updated && !updates_available()` — not applicable since
`Subscription::copy()` calls with `only_if_updated=false`. `DeviceNode::copy(dst, generation)`
(`uORBDeviceNode.hpp:225`) only returns false if `dst == nullptr || _data == nullptr` — both are excluded by the
above. In a single-threaded test (`fd.update()` is never called concurrently with a publish from another thread
in this test suite — confirmed: all Sqe test fixtures publish-then-call-update synchronously, no worker threads),
there is no window for `_data` to become null again between the `.updated()` check and the `.copy()` call a few
instructions later. This matches the reasoning already applied and accepted for FD-D21 by
`work/basis/INVENTORY_FailureDetector_Injector.md` and extends it with the same rigor to FD-D27 (L222), which was
previously only flagged "investigate" — this session's reading confirms the identical mechanism applies to both.
What would reach it: a genuine multi-threaded/multi-process race (a second writer unsubscribing/destroying the
device node between the `.updated()` read and the `.copy()` call) — not reachable from this single-threaded
functional-test harness; would require either a dedicated multi-thread stress test (out of scope for this
assignment's test level, see CLAUDE.md §5's GTest-only, synchronous-fixture convention) or HITL/multi-process SITL
instrumentation.
Counted in feasible coverage? no

---

## Closed this phase (were gaps after IT-1, resolved by IT-2's new tests — listed for traceability, not open gaps)

### (closed) FailureDetector.cpp:212-213 `copy(&imu_status) && (id == selected)` first operand False (FD-D25)
Closed by: SQE-FDI-08 (`FDI08_InstanceAdvertisedButNeverPublished_CopyFailsSearchContinues`,
`SqeFailureDetectorImuTest.cpp`). Unlike G-07 above, this `copy()` call is **not** preceded by an `.updated()`
guard — it follows a successful `ChangeInstance(i)` (which only checks `orb_device_node_exists`, i.e.
advertisement, not whether data was ever published) inside the multi-instance search loop (L205-217), so an
advertised-but-never-published instance genuinely reaches the False side. Proven feasible and closed with a real
test rather than classified as infeasible — see the new test's header comment for the full mechanism (advertise()
without publish() leaves `DeviceNode::_data == nullptr`).

### (closed) FailureDetector.cpp:313 `throttle_above_threshold && current_too_low && !esc_timed_out` (FD-D38,
3rd operand False) and FailureDetector.cpp:324-326 `start_time != 0 && now > ... && (mask & bit) == 0` (FD-D41,
3rd operand False)
Closed by: SQE-FD-36 (`FD36_UnderCurrentConditionButEscAlreadyTimedOut_TimeoutPathFiresNotUnderCurrent`) and
SQE-FD-37 (`FD37_UnderCurrentMaskAlreadyLatched_SecondExpiryIsNoOp`), `SqeFailureDetectorTest.cpp`. Both are
genuinely reachable (not infeasible) — existing tests FD25/FD26 only ever exercised the 1st/2nd operands' False
sides, never the 3rd operand of either 3-way `&&` with the first two held True, which needed a deliberately
engineered esc-timeout-concurrent-with-undercurrent scenario (FD36) and a deliberately re-triggered, already-
latched mask (FD37) respectively.

---

## Reporting note (SPEC_03 / P10 "raw vs feasible" rule)
- **Raw tool numbers** (as `genhtml`/lcov report them, no filtering): see `evidence/coverage/final/per_file.md`.
- **Feasible numbers** (excluding G-01 through G-07's proven-infeasible/environment-limited/tool-artefact items,
  but still counting G-05's exception edges as excluded per-file, consistent with
  `SQE_LCOV_NO_EXCEPTION=1` semantics): computed by hand in `evidence/coverage/compare_baseline_final.md` and the
  final report §6 — every exclusion is justified above, none is asserted without the proof shown.
- gcov's branch-coverage number is condition-level coverage of *evaluated* operands for short-circuit `&&`/`||`
  (CLAUDE.md §8 pitfall 6) — stated once here per SPEC_03's reporting rule, not repeated at every line.
