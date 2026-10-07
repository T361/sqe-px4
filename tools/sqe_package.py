#!/usr/bin/env python3
"""SQE A2 — build the single LMS submission zip: deliverables/<BASE>.zip

Contents (paths inside the zip):
  <BASE>.pdf, <BASE>.xlsx, <BASE>.patch          report, workbook, git patch against v1.17.0
  test_source/                                    student test files + CMake registration, as in the patched tree
  coverage/baseline/, coverage/final/             baseline (upstream only) and final lcov captures, with HTML
  execution_evidence/                             gtest XML, shuffle/individual run logs, sanitizer logs
  tools/                                          scripts referenced by the report's reproduction commands
  README.txt                                      what is where

Usage: python3 tools/sqe_package.py            (run from the repository root)
"""
import os
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def base_name():
    for line in open(os.path.join(ROOT, 'work', 'TEAM.md'), encoding='utf-8'):
        if line.startswith('BASE='):
            return line.strip()[5:]
    sys.exit('BASE= line not found in work/TEAM.md')


def main():
    base = base_name()
    d = os.path.join(ROOT, 'deliverables')
    files = {  # zip path -> repo path
        f'{base}.pdf': f'deliverables/report/{base}.pdf',
        f'{base}.xlsx': f'deliverables/{base}.xlsx',
        f'{base}.patch': f'deliverables/{base}.patch',
    }
    trees = {  # zip dir -> repo dir
        'test_source': 'deliverables/test_source',
        'coverage/baseline': 'evidence/v2/coverage/baseline',
        'coverage/student': 'evidence/v2/coverage/student',
        'coverage/final': 'evidence/v2/coverage/final',
        'execution_evidence/gtest_xml': 'evidence/v2/tests/xml',
        'execution_evidence/run_logs': 'evidence/v2/tests/run',
        'execution_evidence/shuffle_logs': 'evidence/v2/tests/shuffle',
        'execution_evidence/sanitizer': 'evidence/v2/sanitizer',
        'execution_evidence/mutation': 'evidence/v2/mutation',
        'execution_evidence/dryrun': 'evidence/v2/dryrun',
        'execution_evidence/independent_reproduction_original_scope': 'evidence/repro_independent',
    }
    single = {
        'execution_evidence/individual_runs.log': 'evidence/v2/tests/individual.log',
        'execution_evidence/ctest_sqe.log': 'evidence/v2/tests/ctest_sqe.log',
        'execution_evidence/ctest_sqe_junit.xml': 'evidence/v2/tests/ctest_sqe_junit.xml',
        'execution_evidence/final_run.log': 'evidence/v2/final_run.log',
        'execution_evidence/environment.md': 'evidence/v2/env/environment.md',
        'execution_evidence/G-02_delete_null_check.txt': 'evidence/v2/gaps/G-02_delete_null_check.txt',
        'ai_assistance_log.md': 'work/ai_assistance_log.md',
        'tools/sqe_asan_probes.sh': 'tools/sqe_asan_probes.sh',
        'tools/sqe_ubsan_probes.sh': 'tools/sqe_ubsan_probes.sh',
        'tools/sqe_branch_split.py': 'tools/sqe_branch_split.py',
        'tools/sqe_mcdc_check.py': 'tools/sqe_mcdc_check.py',
        'tools/sqe_trace_check.py': 'tools/sqe_trace_check.py',
        'tools/sqe_workbook.py': 'tools/sqe_workbook.py',
        'tools/sqe_reproduce.sh': 'tools/sqe_reproduce.sh',
    }
    readme = f"""SE3002 Software Quality Engineering - Assignment 02 - group {base}
Structural testing and coverage analysis of PX4-Autopilot v1.17.0 (commit d6f12ad1c4f70ad3230afd7d86e971421e02fef4)

{base}.pdf      Report (all required sections; reproduction commands in Appendix A)
{base}.xlsx     Testing workbook: sheet 1 Test Inventory (212 rows), sheet 2 MC-DC Evidence (50 rows)
{base}.patch    Git patch against v1.17.0 (12 test files + 4 CMakeLists.txt; no production code changed)
test_source/            The student test files and modified CMakeLists.txt at their PX4 paths (+ README with commands)
coverage/baseline/      Upstream tests only (before student tests): scope.info, summary.txt, html/index.html
coverage/student/       Student tests only (ctest -R Sqe)
coverage/final/         All tests
execution_evidence/     gtest XML, plain and 10x shuffled runs, each-test-alone log, ctest logs, ASan/UBSan logs,
                        mutation analysis, environment, G-02 disassembly, complete final-run log
ai_assistance_log.md    Full AI-assistance log (the brief record is section 11 of the report)
tools/                  Scripts used by Appendix A; tools/sqe_reproduce.sh = one-command reproduction

Scope: 7 files (DataValidator, DataValidatorGroup, FailureDetector, FailureInjector, battery, LandDetector,
MulticopterLandDetector) - 897 executable lines, 204 decisions, 59 compound.
Final coverage: lines 897/897 (100%); source-level branches 859/863 (99.5%); raw lcov branches 859/1107 (77.6%,
includes 244 compiler-generated exception edges). 210 active tests in 12 binaries, all passing.
"""
    out = os.path.join(d, f'{base}.zip')
    n = 0
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('README.txt', readme)
        for zp, rp in {**files, **single}.items():
            src = os.path.join(ROOT, rp)
            if not os.path.isfile(src):
                sys.exit(f'missing: {rp}')
            z.write(src, zp); n += 1
        for zd, rd in trees.items():
            src_dir = os.path.join(ROOT, rd)
            if not os.path.isdir(src_dir):
                sys.exit(f'missing: {rd}')
            for dirpath, _, names in os.walk(src_dir):
                for name in sorted(names):
                    full = os.path.join(dirpath, name)
                    z.write(full, os.path.join(zd, os.path.relpath(full, src_dir)).replace(os.sep, '/')); n += 1
    print(f'wrote {os.path.relpath(out, ROOT)}: {n + 1} files, {os.path.getsize(out) / 1e6:.1f} MB')
    return 0


if __name__ == '__main__':
    sys.exit(main())
