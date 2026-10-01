#!/usr/bin/env bash
# SQE A2 — build and verify the submission patch against v1.17.0.  Usage: tools/sqe_make_patch.sh build|verify
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$ROOT/PX4-Autopilot}"
BASE="$(sed -n 's/^BASE=//p' "$ROOT/work/TEAM.md" 2>/dev/null | head -1)"; BASE="${BASE:-SQE_A2}"
OUT="$ROOT/deliverables"; PATCH="$OUT/$BASE.patch"
ALLOW='^(src/modules/sensors/data_validator/Sqe[A-Za-z0-9]*Test\.cpp|src/modules/sensors/data_validator/CMakeLists\.txt|src/modules/commander/failure_detector/Sqe[A-Za-z0-9]*Test\.cpp|src/modules/commander/failure_detector/CMakeLists\.txt|cmake/coverage\.cmake)$'
mode="${1:-build}"; mkdir -p "$OUT"
case "$mode" in
build)
  [[ -z "$(git -C "$PX4_DIR" status --porcelain)" ]] || { echo "ERROR: uncommitted changes in PX4-Autopilot — commit on branch sqe-a2 first"; git -C "$PX4_DIR" status --short; exit 1; }
  base="$(git -C "$PX4_DIR" merge-base HEAD 'v1.17.0^{commit}')"
  [[ "$base" == d6f12ad1c4f70ad3230afd7d86e971421e02fef4 ]] || { echo "ERROR: merge-base $base is not v1.17.0"; exit 1; }
  files="$(git -C "$PX4_DIR" diff --name-only v1.17.0 HEAD)"
  bad="$(echo "$files" | grep -Ev "$ALLOW" || true)"
  [[ -z "$bad" ]] || { echo "ERROR: files outside the allow-list (SPEC_07 §1):"; echo "$bad"; exit 1; }
  for cm in $(echo "$files" | grep 'CMakeLists.txt$' || true); do
    removed="$(git -C "$PX4_DIR" diff v1.17.0 HEAD -- "$cm" | grep -E '^-[^-]' || true)"
    [[ -z "$removed" ]] || { echo "ERROR: $cm removes/changes existing lines:"; echo "$removed"; exit 1; }
    added="$(git -C "$PX4_DIR" diff v1.17.0 HEAD -- "$cm" | grep -E '^\+[^+]' | grep -Ev '^\+\s*(#.*)?$|px4_add_(unit|functional)_gtest|^\+\s*(if|endif)\(' || true)"
    [[ -z "$added" ]] || { echo "WARN: unexpected added CMake lines in $cm (review):"; echo "$added"; }
  done
  git -C "$PX4_DIR" diff --binary v1.17.0 HEAD > "$PATCH"
  git -C "$PX4_DIR" diff --stat v1.17.0 HEAD > "$OUT/patch_stat.txt"
  echo "$files" > "$OUT/patch_files.txt"
  echo "wrote $PATCH ($(wc -l < "$PATCH") lines)"; cat "$OUT/patch_stat.txt" ;;
verify)
  [[ -f "$PATCH" ]] || { echo "ERROR: $PATCH missing — run build first"; exit 1; }
  wt="/tmp/sqe-verify-$(date +%s)"; mkdir -p "$ROOT/evidence/gates/G14"; log="$ROOT/evidence/gates/G14/patch_verify.log"
  { echo "# patch verify $(date -u +%FT%TZ)"; set -x
    git -C "$PX4_DIR" worktree add --detach "$wt" v1.17.0
    git -C "$wt" rev-parse HEAD
    git -C "$wt" apply --check "$PATCH"
    git -C "$wt" apply "$PATCH"
    git -C "$wt" status --short
    set +x; } 2>&1 | tee "$log"
  git -C "$PX4_DIR" worktree remove --force "$wt"
  echo "verify OK — log: $log" ;;
*) echo "usage: $0 build|verify"; exit 2 ;;
esac
