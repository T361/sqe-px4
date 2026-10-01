#!/usr/bin/env bash
# SQE A2 — build/run the student test binaries with evidence.  Modes: build | run | probes | shuffle | isolation
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$ROOT/PX4-Autopilot}"; BUILD_DIR="${BUILD_DIR:-$PX4_DIR/build/px4_sitl_test}"
EV="$ROOT/evidence/tests"; TS="$(date -u +%Y%m%dT%H%M%SZ)"; mkdir -p "$EV/xml" "$EV/logs" "$EV/probes"
read -r -a TARGETS <<< "${SQE_TARGETS:-unit-SqeDataValidator unit-SqeDataValidatorGroup unit-SqeDataValidatorGroupAlloc functional-SqeFailureDetector functional-SqeFailureDetectorImu functional-SqeFailureInjector}"
log() { printf '[sqe-tests %s] %s\n' "$(date -u +%H:%M:%S)" "$*" >&2; }
registered() { ( cd "$BUILD_DIR" && ctest -N ) | awk '/Test +#/{print $3}' | grep -E '^(unit|functional)-Sqe' || true; }
binaries() { local t; for t in "${TARGETS[@]}"; do [[ -x "$BUILD_DIR/$t" ]] && echo "$t"; done; }
mode="${1:-run}"
case "$mode" in
build)
  mapfile -t reg < <(registered); log "registered Sqe tests: ${reg[*]:-none (run make tests TESTFILTER=__no_tests__ first)}"
  for t in "${reg[@]}"; do log "building $t"; cmake --build "$BUILD_DIR" --target "$t" 2>&1 | tee -a "$ROOT/evidence/build/sqe_build_$TS.log" | tail -3; done ;;
run)
  fails=0
  for b in $(binaries); do
    log "running $b"; { echo "\$ ./$b --gtest_output=xml:… ($TS)"; ( cd "$BUILD_DIR" && "./$b" --gtest_output="xml:$EV/xml/$b.xml" ); } > "$EV/logs/${b}_$TS.log" 2>&1 || { fails=$((fails+1)); log "  FAILURES in $b (see $EV/logs/${b}_$TS.log)"; }
    grep -E '^\[  (PASSED|FAILED) |YOU HAVE [0-9]+ DISABLED' "$EV/logs/${b}_$TS.log" || true
  done
  ( cd "$BUILD_DIR" && ctest -R Sqe --output-on-failure ) > "$EV/logs/ctest_Sqe_$TS.log" 2>&1 || true
  tail -3 "$EV/logs/ctest_Sqe_$TS.log"; log "binaries with failures: $fails" ;;
probes)
  filter='*PRB*'; [[ "${SQE_SANITIZER:-0}" == 1 ]] || filter='*PRB*:-*PRB05*:*PRB06*'   # PRB05/06 are UB — sanitizer builds only
  for b in $(binaries); do
    ( cd "$BUILD_DIR" && "./$b" --gtest_list_tests --gtest_also_run_disabled_tests --gtest_filter="$filter" ) | grep -q PRB || continue
    log "probes in $b (expected to FAIL)"; ( cd "$BUILD_DIR" && "./$b" --gtest_also_run_disabled_tests --gtest_filter="$filter" --gtest_output="xml:$EV/probes/$b.xml" ) > "$EV/probes/${b}_$TS.log" 2>&1 || true
    grep -E '^\[  (PASSED|FAILED) ' "$EV/probes/${b}_$TS.log" || true
  done ;;
shuffle)
  for b in $(binaries); do rep=5; [[ "$b" == functional-* ]] && rep=10; seed=$(( (RANDOM % 9000) + 1000 ))
    log "$b shuffle x$rep seed $seed"; ( cd "$BUILD_DIR" && "./$b" --gtest_shuffle --gtest_repeat="$rep" --gtest_random_seed="$seed" ) > "$EV/shuffle_${b}_$TS.log" 2>&1 && log "  stable" || log "  UNSTABLE — BLOCKER (see $EV/shuffle_${b}_$TS.log)"
  done ;;
isolation)
  for b in $(binaries); do
    mapfile -t tests < <( cd "$BUILD_DIR" && "./$b" --gtest_list_tests | awk '/^[^ ]/{s=$1} /^  [^ ]/{print s $1}' )
    bad=0; for t in "${tests[@]}"; do ( cd "$BUILD_DIR" && "./$b" --gtest_filter="$t" ) >/dev/null 2>&1 || { bad=$((bad+1)); echo "FAIL alone: $t"; }; done
    log "$b: ${#tests[@]} tests run alone, $bad failed" | tee -a "$EV/isolation_$TS.log"
  done ;;
*) echo "usage: $0 build|run|probes|shuffle|isolation"; exit 2 ;;
esac
