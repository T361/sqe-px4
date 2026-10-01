#!/usr/bin/env python3
"""SQE A2 — lcov tracefile reports.
  summary <info> [--json out.json]         per-file lines/branches/functions (Markdown)
  gaps    <info> [--src-root DIR]          every uncovered line and branch with source text (for GAPS.md / P10)
  compare <baseline.info> <final.info>     per-file before/after table
  lines   <info> [--file SUBSTR] [--range a-b,c-d]   hit count per line (per-test O3 evidence)
"""
import sys, os, json, argparse
from collections import OrderedDict

def parse(path):
    files = OrderedDict(); cur = None
    for raw in open(path, encoding='utf-8', errors='replace'):
        line = raw.strip()
        if line.startswith('SF:'):
            cur = {'path': line[3:], 'DA': {}, 'BR': [], 'FN': {}, 'FNDA': {}}; files[cur['path']] = cur
        elif cur is None:
            continue
        elif line.startswith('DA:'):
            parts = line[3:].split(','); ln, cnt = int(parts[0]), int(float(parts[1]))
            cur['DA'][ln] = cur['DA'].get(ln, 0) + cnt
        elif line.startswith('BRDA:'):
            ln, blk, br, taken = line[5:].split(',', 3)
            cur['BR'].append((int(ln), blk, br, 0 if taken in ('-', '') else int(float(taken)), taken == '-'))
        elif line.startswith('FN:'):
            parts = line[3:].split(','); cur['FN'][parts[-1]] = int(parts[0])
        elif line.startswith('FNDA:'):
            cnt, name = line[5:].split(',', 1); cur['FNDA'][name] = cur['FNDA'].get(name, 0) + int(float(cnt))
        elif line == 'end_of_record':
            cur = None
    return files

def stats(f):
    lf = len(f['DA']); lh = sum(1 for c in f['DA'].values() if c > 0)
    brs = {}
    for ln, blk, br, taken, _ in f['BR']:
        k = (ln, blk, br); brs[k] = brs.get(k, 0) + taken
    bf = len(brs); bh = sum(1 for v in brs.values() if v > 0)
    fnf = len(f['FN']); fnh = sum(1 for n in f['FN'] if f['FNDA'].get(n, 0) > 0)
    return dict(lines_found=lf, lines_hit=lh, branches_found=bf, branches_hit=bh, functions_found=fnf, functions_hit=fnh)

pct = lambda h, t: f"{100.0 * h / t:.1f}%" if t else "n/a"

def cmd_summary(a):
    files = parse(a.info); rows = []; tot = dict.fromkeys(['lines_found','lines_hit','branches_found','branches_hit','functions_found','functions_hit'], 0)
    print(f"# Coverage summary — `{a.info}`\n")
    print("| File | Lines hit/found | Line % | Branches hit/found | Branch % | Functions hit/found |")
    print("|---|---|---|---|---|---|")
    for p, f in files.items():
        s = stats(f); rows.append({'file': p, **s}); [tot.__setitem__(k, tot[k] + s[k]) for k in tot]
        print(f"| {os.path.basename(p)} | {s['lines_hit']}/{s['lines_found']} | {pct(s['lines_hit'], s['lines_found'])} | "
              f"{s['branches_hit']}/{s['branches_found']} | {pct(s['branches_hit'], s['branches_found'])} | {s['functions_hit']}/{s['functions_found']} |")
    print(f"| **Total** | {tot['lines_hit']}/{tot['lines_found']} | {pct(tot['lines_hit'], tot['lines_found'])} | "
          f"{tot['branches_hit']}/{tot['branches_found']} | {pct(tot['branches_hit'], tot['branches_found'])} | {tot['functions_hit']}/{tot['functions_found']} |")
    if not any(r['branches_found'] for r in rows):
        print("\n**WARNING: no branch data (BRF = 0) — branch coverage was not enabled in the capture.**")
    if a.json:
        json.dump({'files': rows, 'total': tot}, open(a.json, 'w'), indent=2)

def src_lines(path, root):
    cands = [path] + ([os.path.join(root, path.lstrip('/'))] if root else [])
    for c in cands:
        if os.path.isfile(c):
            return open(c, encoding='utf-8', errors='replace').read().splitlines()
    return None

def cmd_gaps(a):
    files = parse(a.info); n = 0
    print(f"# Uncovered items — `{a.info}`\nClassify each: missing-test | infeasible | environment-limited | tool-artefact (SPEC_10 §4)\n")
    for p, f in files.items():
        src = src_lines(p, a.src_root); unl = sorted(ln for ln, c in f['DA'].items() if c == 0)
        byline = {}
        for ln, blk, br, taken, never in f['BR']:
            byline.setdefault(ln, []).append((blk, br, taken, never))
        unb = {ln: v for ln, v in byline.items() if any(t == 0 for _, _, t, _ in v)}
        if not unl and not unb:
            print(f"## {os.path.basename(p)} — no gaps\n"); continue
        print(f"## {os.path.basename(p)}")
        for ln in unl:
            n += 1; txt = src[ln-1].strip() if src and 0 < ln <= len(src) else ''
            print(f"- [ ] G-? L{ln} line not executed: `{txt}`")
        for ln in sorted(unb):
            v = unb[ln]; txt = src[ln-1].strip() if src and 0 < ln <= len(src) else ''
            missed = [f"b{blk}.{br}{' (never evaluated)' if nev else ''}" for blk, br, t, nev in v if t == 0]
            n += 1; print(f"- [ ] G-? L{ln} branches missed {len(missed)}/{len(v)} [{', '.join(missed)}]: `{txt}`")
        print()
    print(f"Total uncovered items: {n}")

def cmd_compare(a):
    b, f = parse(a.base), parse(a.final)
    print(f"# Baseline vs final\n- baseline: `{a.base}`\n- final: `{a.final}`\n")
    print("| File | Lines baseline → final | Branches baseline → final |\n|---|---|---|")
    for p in sorted(set(b) | set(f), key=os.path.basename):
        sb = stats(b[p]) if p in b else None; sf = stats(f[p]) if p in f else None
        def fmt(s, k1, k2): return f"{s[k1]}/{s[k2]} ({pct(s[k1], s[k2])})" if s else "absent"
        print(f"| {os.path.basename(p)} | {fmt(sb,'lines_hit','lines_found')} → {fmt(sf,'lines_hit','lines_found')} | "
              f"{fmt(sb,'branches_hit','branches_found')} → {fmt(sf,'branches_hit','branches_found')} |")

def cmd_lines(a):
    files = parse(a.info); ranges = []
    for part in (a.range or '').split(','):
        if part.strip():
            lo, _, hi = part.partition('-'); ranges.append((int(lo), int(hi or lo)))
    for p, f in files.items():
        if a.file and a.file not in p: continue
        print(f"## {p}")
        for ln in sorted(f['DA']):
            if ranges and not any(lo <= ln <= hi for lo, hi in ranges): continue
            brs = [t for l, _, _, t, _ in f['BR'] if l == ln]
            extra = f"  branches taken: {brs}" if brs else ""
            print(f"L{ln}: {f['DA'][ln]}{extra}")

def main():
    ap = argparse.ArgumentParser(); sp = ap.add_subparsers(dest='cmd', required=True)
    s = sp.add_parser('summary'); s.add_argument('info'); s.add_argument('--json')
    g = sp.add_parser('gaps'); g.add_argument('info'); g.add_argument('--src-root')
    c = sp.add_parser('compare'); c.add_argument('base'); c.add_argument('final')
    l = sp.add_parser('lines'); l.add_argument('info'); l.add_argument('--file'); l.add_argument('--range')
    a = ap.parse_args()
    {'summary': cmd_summary, 'gaps': cmd_gaps, 'compare': cmd_compare, 'lines': cmd_lines}[a.cmd](a)

if __name__ == '__main__':
    main()
