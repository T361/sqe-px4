# P14 — Packaging, patch, and reproduction dry run
**Goal:** a submission that can be reconstructed and reproduced from the patch + instructions alone. **Owner:** ORCH/PKG. **Gate:** G14.
**Spec:** SPEC_07.

## Steps
1. Commit everything in the PX4 clone on `sqe-a2` (tests, CMake lines, optional tooling fix as its own commit).
2. `tools/sqe_make_patch.sh build` → `deliverables/<BASE>.patch` (`git diff --binary v1.17.0 HEAD`) + `deliverables/patch_stat.txt`;
   the script refuses if files outside the allow-list changed (Sqe*Test.cpp, the two CMakeLists.txt, optional cmake/coverage.cmake).
3. `tools/sqe_make_patch.sh verify` → creates a fresh worktree at v1.17.0 in `/tmp/sqe-verify-*`, `git apply --check`, applies, lists files.
4. **Dry run (strongly recommended):** in a second clone (or the worktree after `git submodule update --init --recursive`) run the exact
   commands of report Appendix A: build Coverage, run student + final captures, compare `per_file.md` with the submitted numbers (they must match
   except documented timing-dependent SITL baseline effects). Save as `evidence/dryrun/`.
5. Assemble `deliverables/<BASE>.zip`: `tests/` (the Sqe*Test.cpp files + CMake snippets), `coverage/` (baseline, student, final: html + .info +
   MANIFEST), `logs/` (make_tests, run logs, XML, shuffle logs, probes, sanitizer logs if any), `scripts/` (tools/*), `README_REPRODUCE.md`,
   `ai_assistance_record.md`. Keep `<BASE>.pdf`, `<BASE>.xlsx`, `<BASE>.patch` as separate top-level uploads if the LMS allows; otherwise inside the zip.
6. Final checklist = SPEC_07 §5 (every submission-checklist item from the assignment ticked with a path).

## Gate G14 checklist
- [ ] patch verified on fresh v1.17.0; allow-list respected
- [ ] dry-run numbers match (or differences explained)
- [ ] names follow `<Roll1_Roll2_Roll3_Section>.<ext>`
