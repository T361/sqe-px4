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
        'coverage/baseline': 'evidence/coverage/baseline',
        'coverage/final': 'evidence/coverage/final_post_audit',
        'execution_evidence/gtest_xml': 'evidence/tests/xml',
        'execution_evidence/run_logs': 'evidence/tests/logs/shuffle_post_audit',
        'execution_evidence/sanitizer': 'evidence/tests/sanitizer',
        'execution_evidence/independent_reproduction': 'evidence/repro_independent',
    }
    single = {
        'execution_evidence/individual_runs.log': 'evidence/tests/logs/individual_post_audit.log',
        'tools/sqe_asan_probes.sh': 'tools/sqe_asan_probes.sh',
        'tools/sqe_ubsan_probes.sh': 'tools/sqe_ubsan_probes.sh',
        'tools/sqe_branch_split.py': 'tools/sqe_branch_split.py',
        'tools/sqe_mcdc_check.py': 'tools/sqe_mcdc_check.py',
    }
    readme = f"""SE3002 Software Quality Engineering - Assignment 02 - group {base}
Structural testing and coverage analysis of PX4-Autopilot v1.17.0 (commit d6f12ad1c4f70ad3230afd7d86e971421e02fef4)

{base}.pdf      Report (all required sections; reproduction commands in Appendix A)
{base}.xlsx     Testing workbook: sheet 1 Test Inventory, sheet 2 MC-DC Evidence
{base}.patch    Git patch against v1.17.0 (tests + CMake registration only; no production code changed)
test_source/            The student test files and modified CMakeLists.txt at their PX4 paths
coverage/baseline/      Coverage with upstream tests only (before student tests): scope.info, html/index.html
coverage/final/         Final coverage, full suite: scope.info, summary.txt, branch_split.md, html/index.html
                        (student/ = student tests only, identical numbers)
execution_evidence/     gtest XML results, 10x shuffled-repeat and plain run logs, each-test-alone log,
                        ASan/UBSan logs for the F-07 probes, independent second-machine reproduction
tools/                  Scripts used by the commands in the report's Appendix A

Final coverage of the analysed scope (4 files): lines 428/428 (100%); source-level branches 380/384 (99.0%);
raw lcov branches 380/457 (83.2%, includes 73 compiler-generated exception edges). 115 active tests, all passing.
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
