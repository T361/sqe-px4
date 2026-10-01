#!/usr/bin/env python3
"""SQE A2 — split lcov branch records into source-level branches and compiler-generated exception edges.

lcov 2.x marks every exception-unwind edge it reads from gcov with a block ID starting with "e"
(e.g. `BRDA:46,e0,1,0`). This script counts BRDA records per file with and without those edges and lists the
uncovered source-level branches by line. No record is altered or dropped from the input; it only reports.
(lcov's own `--rc no_exception_branch=1` was tried with lcov 2.0-1 + GCC 13 and removed every branch record,
so it could not be used.)

Usage: python3 tools/sqe_branch_split.py evidence/coverage/final_post_audit/scope.info [--md out.md]
"""
import argparse
import sys


def parse(path):
    files, cur = {}, None
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.strip()
        if line.startswith('SF:'):
            cur = line[3:]
            files[cur] = {'DA': {}, 'BRDA': {}}
        elif line.startswith('DA:') and cur:
            ln, cnt = line[3:].split(',')[:2]
            files[cur]['DA'][int(ln)] = files[cur]['DA'].get(int(ln), 0) + int(cnt)
        elif line.startswith('BRDA:') and cur:
            ln, block, br, taken = line[5:].split(',')
            key = (int(ln), block, br)
            files[cur]['BRDA'][key] = files[cur]['BRDA'].get(key, 0) + (0 if taken == '-' else int(taken))
    return files


def pct(h, t):
    return f"{100.0 * h / t:.1f}%" if t else "n/a"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('info')
    ap.add_argument('--md')
    a = ap.parse_args()
    rows, tot = [], [0] * 6
    for path, d in sorted(parse(a.info).items()):
        name = path.rsplit('/', 1)[-1]
        lh, lt = sum(c > 0 for c in d['DA'].values()), len(d['DA'])
        bh, bt = sum(c > 0 for c in d['BRDA'].values()), len(d['BRDA'])
        src = {k: c for k, c in d['BRDA'].items() if not k[1].startswith('e')}
        sh, st = sum(c > 0 for c in src.values()), len(src)
        unc = sorted({k[0] for k, c in src.items() if c == 0})
        rows.append(f"| {name} | {lh}/{lt} ({pct(lh, lt)}) | {bh}/{bt} ({pct(bh, bt)}) | {bt - st} | "
                    f"{sh}/{st} ({pct(sh, st)}) | {', '.join(map(str, unc)) or '-'} |")
        for i, v in enumerate((lh, lt, bh, bt, sh, st)):
            tot[i] += v
    lh, lt, bh, bt, sh, st = tot
    rows.append(f"| **Total** | **{lh}/{lt} ({pct(lh, lt)})** | **{bh}/{bt} ({pct(bh, bt)})** | {bt - st} | "
                f"**{sh}/{st} ({pct(sh, st)})** | |")
    out = '\n'.join([f"# Branch split for `{a.info}`", '',
                     '| File | Lines | Raw branches | Exception edges | Source-level branches | Uncovered source-level lines |',
                     '|---|---|---|---|---|---|'] + rows) + '\n'
    print(out)
    if a.md:
        open(a.md, 'w', encoding='utf-8').write(out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
