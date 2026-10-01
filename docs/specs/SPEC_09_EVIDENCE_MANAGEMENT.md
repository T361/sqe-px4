# SPEC_09 — Evidence management (binding)
```
evidence/
  env/          environment.md, baseline_commit.txt
  baseline/     make_tests.log, ctest_list.txt, LastTest_baseline.log, summary.txt, make_px4_sitl.log (optional)
  build/        coverage_build.log, sqe_build_<ts>.log
  tests/        xml/<binary>.xml, logs/<binary>_<ts>.log, shuffle_<binary>_<ts>.log, probes/…
  coverage/     baseline/ baseline_run2/ IT-1/ … student/ final/ upstream_final/ pertest/ compare_baseline_final.md
  mcdc/         tool/ (optional gcov --conditions output), pertest excerpts used in Sheet 2
  findings/     INV artefacts, sanitizer logs
  gates/Gxx/    outputs of /sqe-gate runs
  dryrun/       P14 reproduction outputs
  tmp/          scratch (never cited)
```
Rules: every log starts with the command line and UTC timestamp (the scripts do this); never overwrite a cited artefact — write a new
timestamped file and update the citation; keep `.info` + HTML for baseline/student/final; delete `.gcda` only via the scripts;
screenshots are optional supplements, never the only evidence.
