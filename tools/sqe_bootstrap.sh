#!/usr/bin/env bash
# SQE A2 — create the workspace skeleton next to CLAUDE.md (idempotent; never overwrites existing work files)
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p work/{scope,basis,mcdc,inventory,findings,explain,audit,viva} \
         evidence/{env,baseline,build,tests/xml,tests/logs,tests/probes,coverage,mcdc/tool,findings,gates,dryrun,tmp} \
         deliverables/report
copy_if_absent() { local src="$1" dst="$2"; if [[ ! -e "$dst" ]]; then cp "$src" "$dst"; echo "created $dst"; else echo "kept    $dst"; fi; }
for f in templates/work/*; do copy_if_absent "$f" "work/$(basename "$f")"; done
copy_if_absent templates/inventory/test_inventory.csv work/inventory/test_inventory.csv
copy_if_absent templates/mcdc/mcdc_matrix.csv work/mcdc/mcdc_matrix.csv
copy_if_absent templates/report/REPORT_TEMPLATE.md deliverables/report/REPORT.md
if [[ ! -e .gitignore ]]; then
  cat > .gitignore <<'EOF'
PX4-Autopilot/
evidence/tmp/
*.gcda
*.gcno
deliverables/*.zip
EOF
  echo "created .gitignore"
fi
chmod +x tools/*.sh tools/*.py 2>/dev/null || true
echo "Bootstrap done. Next: fill work/TEAM.md (BASE=...), then run /sqe-phase P01 in Claude Code."
