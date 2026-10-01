#!/usr/bin/env python3
"""SQE A2 — per-test unique coverage ("what is lost if this test is removed").
Input: a directory of per-test lcov tracefiles (<label>.info, produced by `sqe_coverage.sh pertest-all <binary>`).
Output (Markdown): per test: lines/branches covered, and lines/branch-outcomes covered by NO other test in the set."""
import sys, os, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sqe_lcov_report import parse

def items(info):
    lines, brs = set(), set()
    for p, f in parse(info).items():
        base = os.path.basename(p)
        lines |= {(base, ln) for ln, c in f['DA'].items() if c > 0}
        brs |= {(base, ln, blk, br) for ln, blk, br, t, _ in f['BR'] if t > 0}
    return lines, brs

def main(d):
    files = sorted(glob.glob(os.path.join(d, '*.info')))
    if not files:
        print(f"no .info files in {d}"); return 1
    data = {os.path.basename(f)[:-5]: items(f) for f in files}
    print(f"# Unique coverage per test — {d}\n")
    print("| Test | Lines covered | Branch outcomes covered | Unique lines (lost if removed) | Unique branch outcomes (lost if removed) |")
    print("|---|---|---|---|---|")
    zero = []
    for t, (L, B) in data.items():
        oL = set().union(*(v[0] for k, v in data.items() if k != t)) if len(data) > 1 else set()
        oB = set().union(*(v[1] for k, v in data.items() if k != t)) if len(data) > 1 else set()
        uL, uB = sorted(L - oL), sorted(B - oB)
        fmtL = ', '.join(f"{f}:{ln}" for f, ln in uL[:8]) + (' …' if len(uL) > 8 else '')
        fmtB = ', '.join(f"{f}:{ln} b{blk}.{br}" for f, ln, blk, br in uB[:6]) + (' …' if len(uB) > 6 else '')
        print(f"| {t} | {len(L)} | {len(B)} | {len(uL)} {fmtL} | {len(uB)} {fmtB} |")
        if not uL and not uB:
            zero.append(t)
    print(f"\nTests with no unique structural contribution ({len(zero)}): {', '.join(zero) or 'none'}")
    print("(A test without unique coverage can still be essential: MC/DC pairs and oracles are not measured by line/branch uniqueness.)")
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else '.'))
