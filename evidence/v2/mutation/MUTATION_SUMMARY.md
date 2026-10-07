# Mutation analysis (supplementary test-effectiveness evidence)

Coverage shows that every reachable outcome was *executed*; mutation analysis checks that the assertions would
*notice* a wrong outcome. A mutant is one small operator change in production code (relational `<,<=,>,>=,==,!=` or
logical `&&,||`), rebuilt and run against the student suite; "killed" = at least one test fails. Production code was
restored after every mutant (`git diff` of the production files is empty; evidence/v2/final_run.log was produced after a
full clean relink). Tool: small Python drivers (not part of the deliverable), seeds fixed (20261007).

| Round | Target | Mutants | Invalid (does not compile*) | Valid | Killed | Survived | Score |
|---|---|---|---|---|---|---|---|
| 1 | original 4 files, original suite (audit) | 72 random | 4 | 68 | 56 | 12 | 82.4 % |
| 2 | every `&&`/`\|\|` of the MC/DC decisions DVG-D13/14/15/32/34 + FD L288/L313, parenthesised | 17 | 1 | 16 | 16 | 0 | 100 % |
| 3 | battery.cpp, LandDetector.cpp, MulticopterLandDetector.cpp | 70 random | 10 (+2 changed only a comment) | 58 | 41 | 17 | 70.7 % |
| 3 | re-check: 2 round-1 survivors (DV:82, FI:117) + 2 mutants on the former gaps G-06 (FI:43) / G-07 (FD:194) | 4 | 0 | 4 | 4 | 0 | — |
| 4 | round-3 survivors targeted by new boundary tests | 7 | 0 | 7 | 7 | 0 | — |

\* `&&`→`||` inside a longer `&&` chain triggers `-Werror=parentheses`; round 2 re-did those mutants with correct
parenthesisation so that the logical-operator class is fully exercised.

**Final state.** Original scope: 58/68 valid random mutants killed (85.3 %), 16/16 logical-operator mutants on the MC/DC
decisions killed. New scope: 48/58 killed (82.8 %). The tests added because of mutation analysis are SQE-DV-21
(exact 1e-6 equal-value threshold), SQE-FI-14 (loop must stop at `esc_count`), SQE-BAT-31 (`BAT1_R_INTERNAL` = 0),
SQE-MLD-10/14/19 boundary additions (exact limits), SQE-MLD-25 (stale estimate while not descending), SQE-LD-12
(at-rest follows only the selected gyro).

**Remaining survivors (20) — all classified, none hides an untested behaviour that a test could observe:**

| File:line | Mutation | Class | Reason |
|---|---|---|---|
| DataValidatorGroup.cpp:187 | `conf >= 0.9` → `>` | float-unreachable | confidence = 1 − d/100 never equals 0.9f exactly (0.89999998f, F-06) |
| DataValidatorGroup.cpp:189 | `< 0.01f` → `<=` | float-unreachable | exact 0.01f difference not producible from the confidence formula |
| DataValidatorGroup.cpp:203, :234 | `< FLT_EPSILON` → `<=` | float-unreachable | confidence exactly FLT_EPSILON not producible |
| DataValidatorGroup.cpp:213 | `!= nullptr` → `==` | equivalent | dead False side (G-03) and `reset_state()` is a no-op there (F-14) |
| FailureDetector.cpp:123, :124 | `>= max` → `>` | float-unreachable | Euler angle from a quaternion never equals the radians() limit bit-exactly |
| FailureDetector.cpp:207 | `< ORB_MULTI_MAX_INSTANCES` → `<=` | equivalent | instance 4 does not exist: ChangeInstance(4) fails, loop body skipped |
| FailureDetector.cpp:325 | `now >= start+thr` → `>` | time-equality | needs two wall-clock samples equal to the microsecond |
| FailureDetector.cpp:347 | `< NUM_CONTROLS` → `<=` | UB-only | reads one element past the arrays; observable only under a sanitizer |
| battery.cpp:53 | `index < 1` → `<= 1` | equivalent | index 1 maps to 1 either way |
| battery.cpp:204 | `_dt > FLT_EPSILON` → `>=` | equivalent | dt is a whole number of microseconds; never exactly 1.19e-7 s |
| battery.cpp:253 | `norm < old` → `<=` | float-unreachable | covariance norm bit-identical before/after an RLS step |
| battery.cpp:377 | `age < 2 s` → `<=` | time-equality | flight-phase age exactly 2 000 000 us |
| MulticopterLandDetector.cpp:129 | `> limit` → `>=` | equivalent | clamping to the identical value changes nothing |
| MulticopterLandDetector.cpp:202 | `ALT_GND > 0` → `>= 0` | equivalent | with ALT_GND 0 the next test `dist < 0` is false anyway (distance ≥ 0) |
| MulticopterLandDetector.cpp:209, :283 | `< 1 s` → `<=` | time-equality | data age exactly 1 000 000 us |
| LandDetector.cpp:156 | `>= 1 s` → `>` | time-equality | elapsed exactly 1 000 000 us |
| LandDetector.cpp:223 | `< 4` → `<= 4` | equivalent | instance 4 does not exist |

Raw per-mutant records: `mutation_round1_original_scope.json`, `mutation_round3_new_scope.json`,
`mutation_round4_targeted.json`. Round 2 was run interactively; its 17 results are listed here verbatim:
D15 (K&&L)||M KILLED; D15 K||(L&&M) KILLED; D32 (P&&Q)||R KILLED; D32 P||(Q&&R) KILLED; D34 (P&&Q)||R KILLED;
D34 P||(Q&&R) KILLED; D14 H&&… KILLED; D14 I||J KILLED; D13 A||B KILLED; D13 C||D KILLED; D13 E||F KILLED;
D13 (…)||G KILLED; D13 first ||→&& BUILD_FAIL; FD313 (a&&b)||c KILLED; FD313 a||(b&&c) KILLED; FD288 (a&&b)||c KILLED;
FD288 a||(b&&c) KILLED.
