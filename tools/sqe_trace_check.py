#!/usr/bin/env python3
"""SQE A2 — traceability checks between test code, inventory CSV, MC/DC CSV and XML results.
  --px4 DIR --inventory CSV [--mcdc CSV]   : 1:1 test code <-> inventory; MC/DC test ids exist; comment-ID matches name-ID
  --results DIR [DIR…]                      : list every non-PASS result in gtest XML files
Exit code 0 when no problems."""
import argparse, csv, glob, os, re, sys
import xml.etree.ElementTree as ET

PREFIX = [('DVG', 'SQE-DVG-'), ('DV', 'SQE-DV-'), ('MC', 'SQE-DVG-MC-'), ('AF', 'SQE-DVG-AF-'),
          ('FDI', 'SQE-FDI-'), ('FD', 'SQE-FD-'), ('FI', 'SQE-FI-'), ('PRB', 'SQE-PRB-')]
NAME_RE = re.compile(r'^(DISABLED_)?(DVG|DV|MC|AF|FDI|FD|FI|PRB)(\d{2})_')
TEST_RE = re.compile(r'\bTEST(?:_F)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)')
CMT_RE = re.compile(r'//\s*(SQE-[A-Z]+(?:-[A-Z]+)?-\d{2})')

def id_from_name(name):
    m = NAME_RE.match(name)
    if not m: return None
    return dict(PREFIX)[m.group(2)] + m.group(3)

def scan_code(px4):
    tests = {}
    for f in glob.glob(os.path.join(px4, 'src', '**', 'Sqe*Test.cpp'), recursive=True):
        lines = open(f, encoding='utf-8', errors='replace').read().splitlines()
        for i, line in enumerate(lines):
            m = TEST_RE.search(line)
            if not m: continue
            suite, name = m.groups(); tid = id_from_name(name)
            cmt = None
            for j in range(i - 1, max(-1, i - 30), -1):      # nearest comment above, not crossing the previous TEST
                if TEST_RE.search(lines[j]): break
                mc = CMT_RE.search(lines[j])
                if mc: cmt = mc.group(1); break
            tests[f"{suite}.{name}"] = {'id': tid, 'comment_id': cmt, 'file': os.path.relpath(f, px4), 'line': i + 1}
    return tests

def result_of(tc):
    if tc.find('failure') is not None: return 'FAIL'
    if tc.get('status') == 'notrun': return 'NOT EXECUTED'
    if tc.get('result') == 'skipped' or tc.find('skipped') is not None: return 'NOT EXECUTED (skipped)'
    return 'PASS'

def load_results(paths):
    res = {}
    for p in paths:
        for f in (glob.glob(os.path.join(p, '*.xml')) if os.path.isdir(p) else [p]):
            try: root = ET.parse(f).getroot()
            except ET.ParseError as e: print(f"WARN: cannot parse {f}: {e}"); continue
            for ts in root.iter('testsuite'):
                for tc in ts.iter('testcase'):
                    key = f"{ts.get('name')}.{tc.get('name')}"; r = result_of(tc)
                    if key not in res or res[key][0] == 'NOT EXECUTED':
                        res[key] = (r, os.path.relpath(f))
    return res

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--px4'); ap.add_argument('--inventory'); ap.add_argument('--mcdc')
    ap.add_argument('--results', nargs='*'); a = ap.parse_args(); problems = 0
    if a.results is not None:
        res = load_results(a.results or ['evidence/tests/xml', 'evidence/tests/probes'])
        bad = {k: v for k, v in res.items() if v[0] != 'PASS'}
        print(f"{len(res)} results, {len(bad)} not PASS")
        for k, (r, f) in sorted(bad.items()): print(f"  {r:14s} {k}  ({f})")
        return 0
    if not (a.px4 and a.inventory): ap.error('--px4 and --inventory required (or use --results)')
    code = scan_code(a.px4)
    inv = list(csv.DictReader(open(a.inventory, newline='', encoding='utf-8')))
    inv_ids = [r['test_id'].strip() for r in inv]; inv_by_name = {r['gtest_name'].strip(): r for r in inv if r.get('gtest_name')}
    print(f"test code: {len(code)} tests in Sqe*Test.cpp; inventory: {len(inv)} rows")
    for dup in {i for i in inv_ids if inv_ids.count(i) > 1}: print(f"ERROR duplicate inventory id {dup}"); problems += 1
    for key, t in code.items():
        if t['id'] is None: print(f"ERROR {key}: name does not encode an ID (SPEC_01 §3) [{t['file']}:{t['line']}]"); problems += 1; continue
        if t['comment_id'] and t['comment_id'] != t['id']: print(f"ERROR {key}: comment says {t['comment_id']} but name encodes {t['id']}"); problems += 1
        if not t['comment_id']: print(f"WARN  {key}: no '// SQE-…' comment block above the test")
        if key not in inv_by_name: print(f"ERROR {key} ({t['id']}): missing in inventory"); problems += 1
        elif inv_by_name[key]['test_id'].strip() != t['id']: print(f"ERROR {key}: inventory id {inv_by_name[key]['test_id']} ≠ {t['id']}"); problems += 1
    for name, r in inv_by_name.items():
        if name not in code: print(f"ERROR inventory row {r['test_id']} ({name}) has no test in code"); problems += 1
    if a.mcdc:
        ids = set(inv_ids)
        for r in csv.DictReader(open(a.mcdc, newline='', encoding='utf-8')):
            if r['test_id'].strip() not in ids: print(f"ERROR MC/DC row {r['decision_id']} references unknown test {r['test_id']}"); problems += 1
            for ref in filter(None, (x.strip().split('@')[0] for x in (r.get('pair_with') or '').split(';'))):
                if ref not in ids: print(f"ERROR MC/DC pair reference {ref} unknown"); problems += 1
    print("TRACEABILITY OK" if problems == 0 else f"{problems} problem(s)")
    return 0 if problems == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
