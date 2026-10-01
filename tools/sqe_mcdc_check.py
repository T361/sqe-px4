#!/usr/bin/env python3
"""SQE A2 — MC/DC completeness checker for work/mcdc/mcdc_matrix.csv (schema: docs/specs/SPEC_05 §2).

Checks per decision:
  * consistent number of conditions; every cell is T, F, NE(T), NE(F) or NE
  * every declared pair: partner exists (same decision), outcomes differ, target condition's logical value differs
  * pair form: unique-cause (all other known logical values equal) or masking (others differ) -> reported
  * every condition has >= 1 valid independence pair
  * every condition EVALUATED True and False at least once (executable condition coverage, lecture 7)
  * decision outcome True and False present
Exit 0 only if every decision is complete.  Usage: sqe_mcdc_check.py <matrix.csv> [--report out.md]
"""
import csv, re, sys, argparse
from collections import defaultdict, OrderedDict

CELL = re.compile(r'^(T|F|NE\((T|F)\)|NE)$')

def parse_cell(v):
    v = (v or '').strip().upper().replace(' ', '')
    if v == '':
        return None
    if not CELL.match(v):
        raise ValueError(f"bad cell value '{v}'")
    if v in ('T', 'F'):
        return {'evaluated': True, 'logical': v}
    if v == 'NE':
        return {'evaluated': False, 'logical': None}
    return {'evaluated': False, 'logical': v[3]}

def letters_from_legend(legend, n):
    found = re.findall(r'(?:^|;)\s*([A-Z][A-Z0-9]*)\s*=', legend or '')
    return found[:n] if len(found) >= n else [f"C{i+1}" for i in range(n)]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('csv'); ap.add_argument('--report')
    a = ap.parse_args()
    rows = list(csv.DictReader(open(a.csv, newline='', encoding='utf-8')))
    if not rows:
        print("ERROR: matrix is empty"); return 2
    by_dec = OrderedDict()
    for r in rows:
        by_dec.setdefault(r['decision_id'].strip(), []).append(r)
    out, all_ok = [], True
    for dec, rs in by_dec.items():
        errs, warns = [], []
        n = max(sum(1 for k in range(1, 8) if (r.get(f'c{k}') or '').strip()) for r in rs)
        names = letters_from_legend(rs[0].get('conditions', ''), n)
        parsed = []
        for r in rs:
            try:
                cells = [parse_cell(r.get(f'c{k}')) for k in range(1, n + 1)]
            except ValueError as e:
                errs.append(f"{r['test_id']} {r['evaluation']}: {e}"); continue
            if any(c is None for c in cells):
                errs.append(f"{r['test_id']} {r['evaluation']}: missing condition cell(s)"); continue
            oc = (r.get('outcome') or '').strip().upper()
            if oc not in ('T', 'F'):
                errs.append(f"{r['test_id']} {r['evaluation']}: outcome must be T/F"); continue
            parsed.append({'row': r, 'cells': cells, 'out': oc})
        def find(ref):
            tid, _, ev = ref.strip().partition('@')
            cand = [p for p in parsed if p['row']['test_id'].strip() == tid.strip()]
            if ev:
                cand = [p for p in cand if p['row']['evaluation'].strip().replace(' ', '').startswith(ev.strip().replace(' ', ''))]
            return cand
        pairs = defaultdict(list)
        for p in parsed:
            refs = [x for x in (p['row'].get('pair_with') or '').split(';') if x.strip()]
            shows = [x.strip() for x in (p['row'].get('shows') or '').split(';') if x.strip()]
            if refs and len(refs) != len(shows):
                errs.append(f"{p['row']['test_id']}: pair_with/shows lists differ in length"); continue
            for ref, cond in zip(refs, shows):
                if cond not in names:
                    errs.append(f"{p['row']['test_id']}: unknown condition '{cond}'"); continue
                m = find(ref)
                if len(m) != 1:
                    errs.append(f"{p['row']['test_id']}: partner '{ref}' matches {len(m)} rows"); continue
                q = m[0]; i = names.index(cond)
                if p['out'] == q['out']:
                    errs.append(f"{p['row']['test_id']} vs {ref}: outcomes equal"); continue
                lv1, lv2 = p['cells'][i]['logical'], q['cells'][i]['logical']
                if lv1 is None or lv2 is None or lv1 == lv2:
                    errs.append(f"{p['row']['test_id']} vs {ref}: condition {cond} does not differ (logical {lv1}/{lv2})"); continue
                others = [names[j] for j in range(n) if j != i and p['cells'][j]['logical'] is not None
                          and q['cells'][j]['logical'] is not None and p['cells'][j]['logical'] != q['cells'][j]['logical']]
                unknown = [names[j] for j in range(n) if j != i and (p['cells'][j]['logical'] is None or q['cells'][j]['logical'] is None)]
                form = 'unique-cause' if not others and not unknown else 'masking'
                declared = (p['row'].get('form') or '').strip().lower()
                if form == 'masking' and 'mask' not in declared:
                    warns.append(f"{cond}: {p['row']['test_id']} vs {ref} needs a masking argument (others differ: {others or unknown})")
                pairs[cond].append((p['row']['test_id'], ref, form))
        eval_seen = {c: set() for c in names}
        for p in parsed:
            for j, c in enumerate(names):
                if p['cells'][j]['evaluated']:
                    eval_seen[c].add(p['cells'][j]['logical'])
        outs = {p['out'] for p in parsed}
        missing_pairs = [c for c in names if not pairs[c]]
        missing_tf = [c for c in names if eval_seen[c] != {'T', 'F'}]
        ok = not errs and not missing_pairs and not missing_tf and outs == {'T', 'F'}
        all_ok &= ok
        out.append(f"## {dec}  ({len(parsed)} evaluations, {n} conditions: {', '.join(names)})  -> {'COMPLETE' if ok else 'INCOMPLETE'}")
        for c in names:
            ps = pairs[c]
            desc = '; '.join(f"{a}/{b} [{f}]" for a, b, f in ps[:3]) if ps else 'NO PAIR'
            out.append(f"- {c}: evaluated {''.join(sorted(eval_seen[c])) or '-'} | pairs: {desc}")
        out.append(f"- decision outcomes: {''.join(sorted(outs))}")
        for e in errs: out.append(f"  ERROR: {e}")
        for w in warns: out.append(f"  WARN: {w}")
        if missing_pairs: out.append(f"  ERROR: no independence pair for {missing_pairs}")
        if missing_tf: out.append(f"  ERROR: not evaluated both T and F: {missing_tf}")
        if outs != {'T', 'F'}: out.append("  ERROR: decision outcome not both T and F")
    out.append("\nALL DECISIONS COMPLETE" if all_ok else "\nINCOMPLETE — fix the errors above")
    text = '\n'.join(out); print(text)
    if a.report:
        open(a.report, 'w').write(text + '\n')
    return 0 if all_ok else 1

if __name__ == '__main__':
    sys.exit(main())
