# CFG — `DataValidatorGroup::get_best(uint64_t timestamp, int *index)` (lines 140-246)

```mermaid
flowchart TD
    Start([init: pre_check_best=_curr_best<br/>max_confidence=-1, max_priority=-1000<br/>max_index=-1, best=nullptr]) --> L1{"Loop 1: next != nullptr?<br/>DVG-D10 L157"}
    L1 -->|F, list empty or<br/>_curr_best not found| L1Exit["pre_check_prio stays -1<br/>best stays nullptr"]
    L1 -->|T| L1If{"i == pre_check_best?<br/>DVG-D11 L158"}
    L1If -->|F| L1Next["next = sibling, i++"]
    L1Next --> L1
    L1If -->|T| Seed["seed pre_check_prio/confidence,<br/>max_index/confidence/priority, best<br/>then BREAK"]
    Seed --> L2Start
    L1Exit --> L2Start

    L2Start(["i=0, next=_first"]) --> L2{"Loop 2: next != nullptr?<br/>DVG-D12 L179"}
    L2 -->|F| AfterL2
    L2 -->|T| GetConf["confidence = next->confidence(timestamp)"]
    GetConf --> D13{"DVG-D13 L187-190<br/>((A&&B)||(C&&D)||(E&&F))&&G<br/>— MC/DC target"}
    D13 -->|T| UpdateBest["max_index=i, max_confidence=confidence,<br/>max_priority=next->priority(), best=next"]
    D13 -->|F| L2Next
    UpdateBest --> L2Next["next = sibling, i++"]
    L2Next --> L2

    AfterL2 --> D14{"DVG-D14 L203<br/>max_index != _curr_best ||<br/>(max_confidence<FLT_EPSILON && _curr_best>=0)"}
    D14 -->|F, no change| SetIndex["*index = max_index<br/>return best ? value : nullptr"]
    D14 -->|T, candidate switch| D15{"DVG-D15 L207-208<br/>K&&L&&M<br/>pre_check_prio!=-1 &&<br/>pre_check_prio<max_priority &&<br/>|conf diff|<0.1"}
    D15 -->|T| NotFailover["true_failsafe=false"]
    NotFailover --> D16{"DVG-D16 L213<br/>best != nullptr?<br/>(False is INFEASIBLE here —<br/>see proof below)"}
    D16 -->|T| ResetState["best->reset_state()"]
    D15 -->|F| D17
    ResetState --> D17
    D17{"DVG-D17 L219<br/>_curr_best < 0?"}
    D17 -->|T, first ever| InitBookkeeping["_prev_best = max_index<br/>(no toggle counted)"]
    D17 -->|F, real switch| RealSwitch["_prev_best = pre_check_best"]
    RealSwitch --> D18{"DVG-D18 L226<br/>true_failsafe?"}
    D18 -->|T| Toggle["_toggle_count++"]
    D18 -->|F| SkipToggle["(priority-preferred switch,<br/>not counted as failsafe)"]
    Toggle --> D19{"DVG-D19 L230<br/>_first_failover_time==0?"}
    D19 -->|T| RecordTime["_first_failover_time = timestamp"]
    D19 -->|F| D20
    RecordTime --> D20
    D20{"DVG-D20 L234<br/>max_confidence < FLT_EPSILON?"}
    D20 -->|T| Invalidate["max_index = -1<br/>(all sensors failed)"]
    D20 -->|F| CurrBest
    Invalidate --> CurrBest
    SkipToggle --> CurrBest
    InitBookkeeping --> CurrBest["_curr_best = max_index"]
    CurrBest --> SetIndex
```

## Reading notes
- **Two passes over the sibling list**: Loop 1 finds the *previously* selected sensor's current confidence (the
  "pre-check"), Loop 2 scans every sensor to find the new best by the switch-worthy criteria in D13. A sensor that
  isn't the previous best never gets its confidence computed in Loop 1 — only in Loop 2.
- **D13 is evaluated once per sibling per call**, inside Loop 2 — not once per `get_best()` call. With N siblings,
  D13 is evaluated N times, each with potentially different A-G truth values (since `confidence`, `max_confidence`,
  `next->priority()`, `max_priority` all change across iterations as `best` gets updated). This is why the MC/DC
  independence pairs (REF_06 §3) must be constructed per-call, holding the loop iteration fixed conceptually (i.e.
  as if testing one sensor's effect on one decision evaluation), not confused with "coverage across iterations."
- **DVG-D16 infeasibility proof** (re-derived, not just copied from REF_04): `best` is only non-null-by-construction
  after Loop 1's `Seed` step (L162-168) or after D13's `UpdateBest` step in Loop 2 (L191-195) — both set `best` in the
  same statement block that would also make `pre_check_prio != -1` true (Loop 1's seed sets `pre_check_prio` at
  L162, `best` at L168, same `if` body). So if D15's K (`pre_check_prio != -1`) evaluates True, `best` was
  necessarily assigned during Loop 1 — D16's False branch (`best == nullptr`) is unreachable whenever we get to D16
  at all (D16 is only reached when D15 is True, i.e. K is already True). **Confirmed by reading L156-174 directly.**
- **Early exit via `break`** in both Loop 1 (L169) and the inner `if (i == index)` style in `put()` — once the target
  sibling is found, remaining siblings aren't visited that call. "Not reached" for later siblings in Loop 1 is not
  the same as "condition evaluated False" — it's simply not evaluated at all once `break` fires.
- **Back edge**: both `while` loops' `next = next->sibling(); i++;` tail is the back edge revisiting the loop
  condition (D10/D12) — standard linked-list traversal, terminates because `sibling()` eventually returns `nullptr`
  (enforced by the constructor's `_last->sibling() == nullptr` invariant, never mutated to form a cycle anywhere in
  this file).
