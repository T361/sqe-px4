#!/usr/bin/env bash
# SQE A2 — run the F-07a probe (SQE-PRB-05) under AddressSanitizer without touching the Coverage build.
# PX4's own AddressSanitizer build type does not compile on GCC 13 (bundled abseil/fuzztest), so only
# FailureInjector.cpp and SqeFailureInjectorTest.cpp are recompiled with ASan, into /tmp, and a separate test binary
# is linked from them. Run after the Coverage build that contains the student tests.
# Usage: PX4_BUILD=PX4-Autopilot/build/px4_sitl_test OUT=evidence/tests/sanitizer bash tools/sqe_asan_probes.sh
set -u
OUT=$(realpath -m "${OUT:-evidence/tests/sanitizer}"); mkdir -p "$OUT"
cd "${PX4_BUILD:-PX4-Autopilot/build/px4_sitl_test}"
SAN="-fsanitize=address -fno-omit-frame-pointer"
ninja -t commands functional-SqeFailureInjector > /tmp/sqe_cmds.txt
LD2=$(tail -1 /tmp/sqe_cmds.txt)
for src in 'failure_detector.dir/FailureInjector.cpp.o -c' 'SqeFailureInjectorTest.cpp.o -c'; do
  CC=$(grep -F "$src" /tmp/sqe_cmds.txt | tail -1)
  OBJ=$(echo "$CC" | grep -oE '\-o [^ ]+' | cut -c4-)
  NEW=/tmp/asan_$(basename "$OBJ")
  eval "$(echo "$CC" | sed "s# -o $OBJ # -o $NEW #") $SAN" && echo "COMPILE_OK $NEW"
  LD2=$(echo "$LD2" | sed "s# $OBJ # #")
done
LD2=$(echo "$LD2" | sed -E "s# -o ([^ ]*functional-SqeFailureInjector) # -fsanitize=address -o /tmp/fi_asan /tmp/asan_SqeFailureInjectorTest.cpp.o /tmp/asan_FailureInjector.cpp.o #")
eval "$LD2" && echo LINK_OK
export ASAN_OPTIONS=detect_leaks=0
for p in PRB05 PRB06; do
  /tmp/fi_asan --gtest_also_run_disabled_tests --gtest_filter="*${p}*" > "$OUT/asan_$p.log" 2>&1
  echo "$p exit=$?"; grep -E 'ERROR: AddressSanitizer|READ of|WRITE of|#[0-1] .*FailureInjector|^\[ +(OK|FAILED) +\]' "$OUT/asan_$p.log" | head -6
done
/tmp/fi_asan > "$OUT/asan_active.log" 2>&1
echo "active-suite-under-asan exit=$?"; grep -E 'ERROR: AddressSanitizer|PASSED|FAILED' "$OUT/asan_active.log" | head -5
