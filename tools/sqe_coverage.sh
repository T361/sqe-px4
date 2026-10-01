#!/usr/bin/env bash
# SQE A2 — gcov/lcov coverage pipeline for PX4 v1.17.0 (px4_sitl_test). Spec: docs/specs/SPEC_03_COVERAGE_PIPELINE.md
# Modes: doctor | build | baseline [label] | student [label] | final [label] | capture <label> | pertest <binary> <gtest_filter> <label> | pertest-all <binary>
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$ROOT/PX4-Autopilot}"
BUILD_DIR="${BUILD_DIR:-$PX4_DIR/build/px4_sitl_test}"
EVID="${EVID:-$ROOT/evidence/coverage}"
GCOV_TOOL="${GCOV_TOOL:-gcov}"
JOBS="${JOBS:-}"
SQE_CAPTURE="${SQE_CAPTURE:-scope}"
if [[ -n "${SQE_SCOPE_PATTERNS:-}" ]]; then IFS=':' read -r -a SCOPE_PATTERNS <<< "$SQE_SCOPE_PATTERNS"; else
SCOPE_PATTERNS=('*/sensors/data_validator/DataValidator.cpp' '*/sensors/data_validator/DataValidatorGroup.cpp'
                '*/commander/failure_detector/FailureDetector.cpp' '*/commander/failure_detector/FailureInjector.cpp'); fi
if [[ -n "${SQE_OBJDIRS:-}" ]]; then IFS=':' read -r -a OBJDIRS <<< "$SQE_OBJDIRS"; else
OBJDIRS=("$BUILD_DIR/src/modules/sensors/data_validator" "$BUILD_DIR/src/modules/commander/failure_detector"); fi

log()  { printf '[sqe-cov %s] %s\n' "$(date -u +%H:%M:%S)" "$*" >&2; }
die()  { log "ERROR: $*"; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "$1 not found (install lcov; see P01)"; }

find_lcovrc() {
  local c
  for c in "${LCOVRC:-}" "$HOME/.lcovrc" /etc/lcovrc /usr/local/etc/lcovrc /opt/homebrew/etc/lcovrc \
           "$(dirname "$(command -v lcov)")/../etc/lcovrc"; do
    [[ -n "$c" && -f "$c" ]] && { echo "$c"; return 0; }
  done; return 1
}
rc_has() { local f; f="$(find_lcovrc)" || return 1; grep -qE "^[#[:space:]]*$1[[:space:]]*=" "$f"; }
setup_flags() {
  need lcov; need genhtml
  LCOV_VER="$(lcov --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -1)"; local major="${LCOV_VER%%.*}" minor="${LCOV_VER#*.}"
  if rc_has branch_coverage; then BR=branch_coverage
  elif rc_has lcov_branch_coverage; then BR=lcov_branch_coverage
  elif (( major > 2 || (major == 2 && minor >= 1) )); then BR=branch_coverage
  else BR=lcov_branch_coverage; fi
  if rc_has no_exception_branch; then EXC=no_exception_branch
  elif rc_has geninfo_no_exception_branch; then EXC=geninfo_no_exception_branch
  elif (( major >= 2 )); then EXC=no_exception_branch; else EXC=geninfo_no_exception_branch; fi
  RC=(--rc "$BR=1")
  # Exception-branch filtering is OPT-IN: with lcov 2.0 + gcov 13 '--rc no_exception_branch=1' removed ALL branch records in testing.
  [[ "${SQE_LCOV_NO_EXCEPTION:-0}" == 1 ]] && RC+=(--rc "$EXC=1")
  IGN=()
  if (( major >= 2 )); then
    local cats="mismatch${SQE_LCOV_EXTRA_IGNORE:+,$SQE_LCOV_EXTRA_IGNORE}"
    IGN=(--ignore-errors "$cats")
    [[ -n "${SQE_LCOV_FILTER:-}" ]] && IGN+=(--filter "$SQE_LCOV_FILTER")
  fi
  DIRARGS=()
  if [[ "$SQE_CAPTURE" == all ]]; then DIRARGS=(--directory "$BUILD_DIR"); else
    local d; for d in "${OBJDIRS[@]}"; do [[ -d "$d" ]] && DIRARGS+=(--directory "$d") || log "WARN: objdir missing: $d"; done
    (( ${#DIRARGS[@]} )) || die "no object directories found — is the Coverage build done? ($BUILD_DIR)"
  fi
}
zero() { local n=0 d; if [[ "$SQE_CAPTURE" == all ]]; then n=$(find "$BUILD_DIR" -name '*.gcda' | wc -l); find "$BUILD_DIR" -name '*.gcda' -delete
         else for d in "${OBJDIRS[@]}"; do [[ -d "$d" ]] || continue; n=$((n + $(find "$d" -name '*.gcda' | wc -l))); find "$d" -name '*.gcda' -delete; done; fi
         log "zeroed $n .gcda files"; }
run_ctest() { local out="$1"; shift; local rc=0
  mkdir -p "$out"; log "ctest $*"
  local junit=(); ctest --help 2>/dev/null | grep -q -- '--output-junit' && junit=(--output-junit "$out/ctest_junit.xml")
  ( cd "$BUILD_DIR" && ctest --output-on-failure -T Test "${junit[@]}" "$@" ) > "$out/ctest.log" 2>&1 || rc=$?
  echo "$rc" > "$out/ctest_exit_code"; log "ctest exit code $rc (non-zero means failures — capture continues)"; }
capture() { local out="$1" label="$2"
  mkdir -p "$out"
  log "capture initial (zero records) …"
  lcov --capture --initial "${DIRARGS[@]}" --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" "${RC[@]}" "${IGN[@]}" -o "$out/initial.info" > "$out/lcov_initial.log" 2>&1 || { tail -20 "$out/lcov_initial.log" >&2; die "initial capture failed (see $out/lcov_initial.log)"; }
  log "capture run data …"
  if lcov --capture "${DIRARGS[@]}" --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" "${RC[@]}" "${IGN[@]}" -o "$out/run.info" > "$out/lcov_run.log" 2>&1; then
    if ! grep -q '^BRDA' "$out/run.info" && [[ "${SQE_LCOV_NO_EXCEPTION:-0}" == 1 ]]; then
      log "WARN: exception filter removed all branch data — retrying without it (recorded in MANIFEST)"; RC=(--rc "$BR=1"); FALLBACK_NOTE="exception filter dropped all branches; disabled"
      lcov --capture --initial "${DIRARGS[@]}" --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" "${RC[@]}" "${IGN[@]}" -o "$out/initial.info" > "$out/lcov_initial.log" 2>&1
      lcov --capture "${DIRARGS[@]}" --base-directory "$BUILD_DIR" --gcov-tool "$GCOV_TOOL" "${RC[@]}" "${IGN[@]}" -o "$out/run.info" > "$out/lcov_run.log" 2>&1
    fi
    lcov -a "$out/initial.info" -a "$out/run.info" "${RC[@]}" "${IGN[@]}" -o "$out/all.info" > "$out/lcov_merge.log" 2>&1 || die "merge failed ($out/lcov_merge.log)"
  else log "WARN: no run data captured (no .gcda?) — using zero records only"; cp "$out/initial.info" "$out/all.info"; fi
  lcov --extract "$out/all.info" "${SCOPE_PATTERNS[@]}" "${RC[@]}" "${IGN[@]}" -o "$out/scope.info" > "$out/lcov_extract.log" 2>&1 || die "extract failed ($out/lcov_extract.log)"
  grep -q '^SF:' "$out/scope.info" || die "scope.info has no files — check SCOPE_PATTERNS / objdirs"
  grep -q '^BRDA' "$out/scope.info" || die "scope.info has NO branch data (BRF=0) — branch coverage flags not effective; see REF_10 #23"
  lcov --summary "$out/scope.info" "${RC[@]}" "${IGN[@]}" > "$out/summary.txt" 2>&1 || true
  python3 "$ROOT/tools/sqe_lcov_report.py" summary "$out/scope.info" --json "$out/per_file.json" > "$out/per_file.md"
  local prefix=(); [[ -d "$PX4_DIR" ]] && prefix=(--prefix "$PX4_DIR")
  if ! genhtml "$out/scope.info" --branch-coverage --legend "${prefix[@]}" --title "SQE A2 $label" -o "$out/html" "${RC[@]}" "${IGN[@]}" > "$out/genhtml.log" 2>&1; then
    log "WARN: genhtml with full options failed, retrying minimal"; genhtml "$out/scope.info" --branch-coverage -o "$out/html" > "$out/genhtml.log" 2>&1 || log "WARN: genhtml failed ($out/genhtml.log)"; fi
  manifest "$out" "$label"; log "done → $out (per_file.md, html/index.html)"; cat "$out/per_file.md"; }
manifest() { local out="$1" label="$2"
  { echo "# Coverage capture manifest — $label"; echo "UTC: $(date -u +%FT%TZ)"
    echo "Command: $0 ${SQE_ARGS:-}"
    [[ -d "$PX4_DIR/.git" ]] && { echo "PX4 HEAD: $(git -C "$PX4_DIR" rev-parse HEAD) (branch $(git -C "$PX4_DIR" branch --show-current))"; echo "Dirty files: $(git -C "$PX4_DIR" status --porcelain | wc -l)"; }
    [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && echo "CMAKE_BUILD_TYPE: $(grep -m1 '^CMAKE_BUILD_TYPE' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)"
    echo "lcov: $(lcov --version | head -1) · genhtml: $(genhtml --version | head -1) · gcov tool: $GCOV_TOOL ($($GCOV_TOOL --version 2>/dev/null | head -1))"
    echo "lcovrc: $(find_lcovrc || echo none) · rc flags: ${RC[*]} · ignore/filter: ${IGN[*]:-none}"
    echo "capture dirs: ${DIRARGS[*]}"; echo "scope patterns: ${SCOPE_PATTERNS[*]}"
    [[ -f "$out/ctest_exit_code" ]] && { echo "ctest args: ${CTEST_ARGS:-}"; echo "ctest exit code: $(cat "$out/ctest_exit_code")"; echo "tests run: $(grep -cE '^ *[0-9]+/[0-9]+ Test' "$out/ctest.log" 2>/dev/null || echo ?)"; }
    echo "Notes: ${FALLBACK_NOTE:-none}"; } > "$out/MANIFEST.md"; }

pertest_one() { local bin="$1" filter="$2" label="$3" out="$4" tmp
  mkdir -p "$out"; zero
  ( cd "$BUILD_DIR" && "./$bin" --gtest_filter="$filter" ) > "$out/$label.log" 2>&1 || log "WARN: $label exit code non-zero (see $out/$label.log)"
  tmp="$out/.tmp_$label"; capture "$tmp" "$label" >/dev/null
  cp "$tmp/scope.info" "$out/$label.info"; python3 "$ROOT/tools/sqe_lcov_report.py" lines "$out/$label.info" > "$out/${label}_lines.txt"; rm -rf "$tmp"
  log "per-test coverage → $out/$label.info"; }
SQE_ARGS="$*"; mode="${1:-}"; shift || true
case "$mode" in
doctor) setup_flags; echo "lcov $LCOV_VER; lcovrc $(find_lcovrc || echo none); flags: ${RC[*]} ${IGN[*]:-}"; echo "gcov tool: $GCOV_TOOL — $($GCOV_TOOL --version 2>/dev/null | head -1)"
        [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && grep -m1 '^CMAKE_BUILD_TYPE' "$BUILD_DIR/CMakeCache.txt" || echo "no CMakeCache at $BUILD_DIR"
        for d in "${OBJDIRS[@]}"; do echo "objdir $d: $(find "$d" -name '*.gcno' 2>/dev/null | wc -l) gcno, $(find "$d" -name '*.gcda' 2>/dev/null | wc -l) gcda"; done ;;
build)  mkdir -p "$ROOT/evidence/build"; ( cd "$PX4_DIR" && { echo "\$ make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__ ${JOBS:+-j$JOBS}  ($(date -u +%FT%TZ))"; make tests PX4_CMAKE_BUILD_TYPE=Coverage TESTFILTER=__no_tests__ ${JOBS:+-j"$JOBS"}; } ) 2>&1 | tee "$ROOT/evidence/build/coverage_build.log"
        grep -m1 '^CMAKE_BUILD_TYPE' "$BUILD_DIR/CMakeCache.txt" ;;
baseline|student|final)
        label="${1:-$mode}"; out="$EVID/$label"; setup_flags; zero
        case "$mode" in baseline) CTEST_ARGS="-E Sqe|antlr4_tests_NOT_BUILT";; student) CTEST_ARGS="-R Sqe";; final) CTEST_ARGS="-E antlr4_tests_NOT_BUILT";; esac
        read -r -a ca <<< "$CTEST_ARGS"; run_ctest "$out" "${ca[@]}"; capture "$out" "$label" ;;
capture) label="${1:?label}"; setup_flags; capture "$EVID/$label" "$label" ;;
pertest) bin="${1:?binary}"; filter="${2:?gtest filter}"; label="${3:?label}"; setup_flags
        pertest_one "$bin" "$filter" "$label" "$EVID/pertest" ;;
pertest-all) bin="${1:?binary}"; out="$EVID/pertest/$bin"; mkdir -p "$out"; setup_flags
        mapfile -t tests < <( cd "$BUILD_DIR" && "./$bin" --gtest_list_tests | awk '/^[^ ]/{s=$1} /^  [^ ]/{print s $1}' )
        log "${#tests[@]} tests in $bin"
        for t in "${tests[@]}"; do pertest_one "$bin" "$t" "${t//[^A-Za-z0-9_]/_}" "$out" || log "WARN: $t"; done
        python3 "$ROOT/tools/sqe_unique_coverage.py" "$out" > "$out/unique_coverage.md" && log "unique coverage → $out/unique_coverage.md" ;;
*) sed -n '2,4p' "$0"; exit 2 ;;
esac
