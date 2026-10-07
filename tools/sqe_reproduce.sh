#!/usr/bin/env bash
# SQE A2 — one-command reproduction of the submission on any Ubuntu 24.04 / WSL2 machine.
# Usage:  bash sqe_reproduce.sh /path/to/24i3015_24i3166_24i3158_B.zip [workdir]
# Does: fresh clone of PX4 v1.17.0, official toolchain setup, apply the zipped patch, Coverage build,
#       ctest -R Sqe, lcov capture of the 7-file scope, and compares with the expected numbers.
set -u
ZIP=$(realpath "${1:?usage: sqe_reproduce.sh <submission.zip> [workdir]}")
WD=$(realpath -m "${2:-$HOME/sqe_repro}")
BASE=24i3015_24i3166_24i3158_B
EXPECT_LINES="897/897"; EXPECT_BR="859/863"; EXPECT_BIN=12
mkdir -p "$WD" && cd "$WD" || exit 1
echo "== [1/6] unpack submission"; rm -rf zip && mkdir zip && python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall('zip')" "$ZIP"
echo "== [2/6] clone PX4 v1.17.0 (recursive, ~3 GB)"
[ -d PX4 ] || git clone -q --branch v1.17.0 --recursive https://github.com/PX4/PX4-Autopilot.git PX4 || exit 1
cd PX4; HEAD=$(git rev-parse HEAD); echo "   HEAD $HEAD"
[ "$HEAD" = d6f12ad1c4f70ad3230afd7d86e971421e02fef4 ] || { echo "WRONG BASELINE"; exit 1; }
echo "== [3/6] toolchain (sudo needed once)"
command -v lcov >/dev/null && command -v ninja >/dev/null || bash Tools/setup/ubuntu.sh --no-nuttx --no-sim-tools
echo "== [4/6] apply patch + Coverage build"
git apply --check "../zip/$BASE.patch" 2>/dev/null && git apply "../zip/$BASE.patch"
PX4_CMAKE_BUILD_TYPE=Coverage make tests TESTFILTER=__no_tests__ > ../build.log 2>&1 || { tail -30 ../build.log; exit 1; }
cd build/px4_sitl_test
LC="--rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,negative,empty,source,gcov,deprecated"
lcov $LC --zerocounters -d . > /dev/null 2>&1
echo "== [5/6] ctest -R Sqe"
ctest -R Sqe > "$WD/ctest.log" 2>&1; tail -4 "$WD/ctest.log"
NBIN=$(grep -cE 'Test +#[0-9]+: .*Sqe.* Passed' "$WD/ctest.log")
echo "== [6/6] coverage of the 7-file scope"
lcov $LC --capture -d . -o "$WD/all.info" > /dev/null 2>&1
lcov $LC --extract "$WD/all.info" '*/data_validator/DataValidator.cpp' '*/data_validator/DataValidatorGroup.cpp' \
  '*/failure_detector/FailureDetector.cpp' '*/failure_detector/FailureInjector.cpp' '*/lib/battery/battery.cpp' \
  '*/land_detector/LandDetector.cpp' '*/land_detector/MulticopterLandDetector.cpp' -o "$WD/student.info" > /dev/null 2>&1
read LINES BR < <(python3 - "$WD/student.info" <<'EOF'
import sys
L=H=B=BH=0
for ln in open(sys.argv[1]):
    if ln.startswith('DA:'): L += 1; H += int(ln.strip().split(',')[1]) > 0
    elif ln.startswith('BRDA:'):
        _, blk, _, t = ln[5:].strip().split(',')
        if not blk.startswith('e'): B += 1; BH += (t != '-' and int(t) > 0)
print(f"{H}/{L} {BH}/{B}")
EOF
)
echo "   binaries passed: $NBIN (expected $EXPECT_BIN) | lines $LINES (expected $EXPECT_LINES) | source-level branches $BR (expected $EXPECT_BR)"
if [ "$NBIN" = "$EXPECT_BIN" ] && [ "$LINES" = "$EXPECT_LINES" ] && [ "$BR" = "$EXPECT_BR" ]; then
  echo "REPRODUCED OK on $(hostname) $(date -Is)"; exit 0
else
  echo "MISMATCH — keep $WD/ctest.log and $WD/build.log and investigate"; exit 2
fi
