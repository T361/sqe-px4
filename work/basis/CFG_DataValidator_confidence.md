# CFG — `DataValidator::confidence(uint64_t timestamp)` (lines 100-142)

```mermaid
flowchart TD
    Start([ret = 1.0f]) --> D07{"_time_last == 0?<br/>DV-D07 L106"}
    D07 -->|T| NoData["mask |= NO_DATA<br/>ret = 0.0f"]
    NoData --> D12
    D07 -->|F| D08{"timestamp > _time_last<br/>+ timeout? DV-D08 L110"}
    D08 -->|T| Timeout["mask |= TIMEOUT<br/>ret = 0.0f"]
    Timeout --> D12
    D08 -->|F| D09{"_value_equal_count ><br/>threshold? DV-D09 L115"}
    D09 -->|T| Stale["mask |= STALE_DATA<br/>ret = 0.0f"]
    Stale --> D12
    D09 -->|F| D10{"_error_count ><br/>NORETURN_ERRCOUNT?<br/>DV-D10 L120"}
    D10 -->|T| HighErrCnt["mask |= HIGH_ERRCOUNT<br/>ret = 0.0f"]
    HighErrCnt --> D12
    D10 -->|F| D11{"_error_density ><br/>ERROR_DENSITY_WINDOW?<br/>DV-D11 L125"}
    D11 -->|T| HighDensity["mask |= HIGH_ERRDENSITY<br/>density = WINDOW (cap)<br/>ret STAYS 1.0f here"]
    HighDensity --> D12
    D11 -->|F| D12{"ret > 0.0f?<br/>DV-D12 L132<br/>(outer if)"}
    D12 -->|T| Compute["ret = 1 - density/WINDOW"]
    Compute --> D13{"ret > 0.0f?<br/>DV-D13 L136<br/>(nested if)"}
    D13 -->|T| ClearMask["mask = NO_ERROR"]
    D13 -->|F| KeepMask["mask kept<br/>(density >= 100 exactly)"]
    D12 -->|F| Return(["return ret"])
    ClearMask --> Return
    KeepMask --> Return
```

## Reading notes
- The five `if`/`else-if` branches (D07-D11) are **mutually exclusive** — only one of NO_DATA / TIMEOUT / STALE_DATA /
  HIGH_ERRCOUNT / HIGH_ERRDENSITY can fire per call, because they're chained with `else if`. This matters for test
  design: you cannot reach D09 (STALE) without first making D07 and D08 both False.
- **D11's True branch is a trap for naive testers**: unlike D07-D10, triggering HIGH_ERRDENSITY does *not* set
  `ret = 0.0f`. It caps `_error_density` but leaves `ret` at its initial `1.0f`, so execution falls into the D12 `True`
  branch and `ret` gets recomputed as `1 - density/WINDOW` — which, since density is now capped exactly at
  `ERROR_DENSITY_WINDOW` (100.0f), evaluates to `1 - 100/100 = 0.0f`. So D13 (nested) is reached with `ret == 0.0f`,
  taking the **False** branch (mask kept, not cleared) — this is the "density == 100 → F" case REF_03 flags at D11/D13.
  A test must distinguish "ret became 0 via the early-exit branches" from "ret became 0 via the D12/D13 computed path"
  to actually exercise this edge.
- **Early exit vs. fall-through**: D07/D08/D09/D10 are early-exit-style (each sets `ret=0.0f` and the mask, then the
  chain ends because of `else if`), but there's no `return` — execution always reaches D12. D12's condition
  (`ret > 0.0f`) is what actually distinguishes "no error occurred above" (ret still 1.0f, or became 0 via D11
  specifically) from "a hard error occurred" (ret forced to 0.0f by D07-D10) and skips the whole recompute block for
  the latter.
- "Not reached ≠ False": the `print()` function (DV-D14/D15) calls `confidence(hrt_absolute_time())` again, as a
  *side effect of formatting output* — this means calling `print()` can itself change `_error_mask` (via D13), which
  is a real side-effect trap: a test that calls `print()` for diagnostic purposes between two confidence-affecting
  assertions can inadvertently mutate state (flagged in REF_03 as side effect F-05, cross-referenced from DV-D15).
