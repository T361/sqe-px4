#!/usr/bin/env bash
# SQE A2 — run the F-07b probe (SQE-PRB-06) under UBSan (-fsanitize=shift,bounds) without touching the Coverage build.
# PX4's own UndefinedBehaviorSanitizer build type does not compile on GCC 13 (bundled abseil/fuzztest), so only
# FailureInjector.cpp is recompiled with UBSan, into /tmp, and a separate test binary is linked with it.
# Run after the Coverage build that contains the student tests.
# Usage: PX4_BUILD=PX4-Autopilot/build/px4_sitl_test OUT=evidence/tests/sanitizer bash tools/sqe_ubsan_probes.sh
set -u
OUT=$(realpath -m "${OUT:-evidence/tests/sanitizer}"); mkdir -p "$OUT"
cd "${PX4_BUILD:-PX4-Autopilot/build/px4_sitl_test}"
SAN="-fsanitize=shift,bounds -fno-sanitize-recover=shift,bounds"
ninja -t commands functional-SqeFailureInjector > /tmp/sqe_cmds.txt
CC_CMD=$(grep -F 'failure_detector.dir/FailureInjector.cpp.o -c' /tmp/sqe_cmds.txt | tail -1)
LD_CMD=$(tail -1 /tmp/sqe_cmds.txt)
OBJ=$(echo "$CC_CMD" | grep -oE '\-o [^ ]+' | cut -c4-)
eval "$(echo "$CC_CMD" | sed "s# -o $OBJ # -o /tmp/FI_ubsan.o #") $SAN" && echo COMPILE_OK
LD2=$(echo "$LD_CMD" | sed -E "s# -o ([^ ]*functional-SqeFailureInjector) # -o /tmp/fi_ubsan /tmp/FI_ubsan.o -fsanitize=undefined -lubsan #")
eval "$LD2" && echo LINK_OK
for p in PRB05 PRB06; do
  /tmp/fi_ubsan --gtest_also_run_disabled_tests --gtest_filter="*${p}*" > "$OUT/ubsan_$p.log" 2>&1
  echo "$p exit=$?"; grep -E 'runtime error|^\[ +(OK|FAILED) +\]' "$OUT/ubsan_$p.log"
done
/tmp/fi_ubsan > "$OUT/ubsan_active.log" 2>&1
echo "active-suite-under-ubsan exit=$?"; grep -E 'runtime error|PASSED|FAILED' "$OUT/ubsan_active.log" | head -5
