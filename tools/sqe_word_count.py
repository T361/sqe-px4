#!/usr/bin/env python3
"""SQE A2 — report word counts. Counts prose words (excluding code blocks, HTML comments, table separator rows, Appendix sections)
and the final judgment between <!-- JUDGMENT-START --> and <!-- JUDGMENT-END --> (must be 300–400)."""
import re, sys
def words(text):
    text = re.sub(r'```.*?```', ' ', text, flags=re.S); text = re.sub(r'<!--.*?-->', ' ', text, flags=re.S)
    text = re.sub(r'^\s*\|?\s*:?-{3,}.*$', ' ', text, flags=re.M)
    return len(re.findall(r"[A-Za-z0-9][A-Za-z0-9'’\-./%]*", text))
def main(p):
    s = open(p, encoding='utf-8').read()
    m = re.search(r'<!--\s*JUDGMENT-START\s*-->(.*?)<!--\s*JUDGMENT-END\s*-->', s, flags=re.S)
    body = re.split(r'^#+\s*Appendix', s, flags=re.M | re.I)[0]
    print(f"report prose words (before appendices): {words(body)}")
    if not m: print("ERROR: judgment markers not found"); return 1
    j = words(m.group(1)); ok = 300 <= j <= 400
    print(f"final judgment words: {j} -> {'OK' if ok else 'OUT OF RANGE (300–400)'}"); return 0 if ok else 1
if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'deliverables/report/REPORT.md'))
