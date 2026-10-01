# SPEC_07 — Patch and submission (binding)

## 1. Allow-list of changed paths (vs `v1.17.0`)
- `src/modules/sensors/data_validator/Sqe*Test.cpp` (new) · `src/modules/sensors/data_validator/CMakeLists.txt` (appended gtest lines only)
- `src/modules/commander/failure_detector/Sqe*Test.cpp` (new) · `src/modules/commander/failure_detector/CMakeLists.txt` (appended lines only)
- `cmake/coverage.cmake` — only the documented Clang typo fix (TOOLING-01), only if needed on macOS
Anything else → the patch script aborts. Production `.cpp/.hpp` must be byte-identical to v1.17.0.

## 2. Creating the patch
`tools/sqe_make_patch.sh build` → `git -C PX4-Autopilot diff --binary v1.17.0 HEAD > deliverables/<BASE>.patch`, plus `patch_stat.txt`
(`git diff --stat`) and `patch_files.txt`. Uncommitted changes abort the script (commit first). The diff is taken against the **tag** so that the
file header shows exactly what the grader must apply on a fresh `git clone --branch v1.17.0 --recursive`.

## 3. Verifying the patch
`tools/sqe_make_patch.sh verify` → `git worktree add --detach /tmp/sqe-verify-<ts> v1.17.0` → `git apply --check` → `git apply` → list changed
files → remove the worktree. Save the transcript as `evidence/gates/G14/patch_verify.log`.

## 4. Bundle `deliverables/<BASE>.zip`
```
<BASE>/README_REPRODUCE.md        exact commands (= report Appendix A)
<BASE>/tests/                     Sqe*Test.cpp files + the two CMake snippets
<BASE>/coverage/baseline|student|final/   html/ + scope.info + per_file.md + MANIFEST.md
<BASE>/logs/                      make_tests baseline log, coverage build log, test XML + logs, shuffle logs, probe logs, sanitizer logs
<BASE>/scripts/                   tools/*
<BASE>/ai_assistance_record.md
```
Top-level uploads: `<BASE>.pdf`, `<BASE>.xlsx`, `<BASE>.patch`, `<BASE>.zip` (or everything inside the zip if the LMS takes one file).

## 5. Final checklist (assignment "Submission Checklist" → artefact)
- [ ] Report: group details, environment, tag/commit, analysis & scope, approach, MC/DC, coverage analysis, defects, gaps/limitations, 300–400-word judgment → `<BASE>.pdf`
- [ ] Student-authored test files + CMake registration → zip `tests/` + patch
- [ ] Git diff/patch vs v1.17.0 → `<BASE>.patch` (verified)
- [ ] Setup/run instructions with exact commands → `README_REPRODUCE.md` + report Appendix A
- [ ] Baseline and final coverage evidence (HTML + .info) → zip `coverage/`
- [ ] Test execution evidence (logs/XML, not screenshots only) → zip `logs/`
- [ ] Workbook, ≤ 2 sheets → `<BASE>.xlsx`
- [ ] Brief AI-assistance record → report §10 + `ai_assistance_record.md`
- [ ] Naming convention respected
