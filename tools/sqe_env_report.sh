#!/usr/bin/env bash
# SQE A2 — print a Markdown environment report (redirect to evidence/env/environment.md)
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PX4_DIR="${PX4_DIR:-$ROOT/PX4-Autopilot}"
v() { local out; out="$("$@" 2>&1 | head -1)" || true; [[ -z "$out" ]] && out="not found"; echo "$out"; }
os="$(uname -s)"
echo "# Environment report"
echo "Generated (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
echo "| Item | Value |"
echo "|---|---|"
if [[ "$os" == "Darwin" ]]; then
  echo "| OS | macOS $(sw_vers -productVersion 2>/dev/null) ($(sw_vers -buildVersion 2>/dev/null)) |"
  echo "| CPU | $(sysctl -n machdep.cpu.brand_string 2>/dev/null) / $(sysctl -n hw.ncpu 2>/dev/null) cores |"
  echo "| RAM | $(( $(sysctl -n hw.memsize 2>/dev/null || echo 0) / 1024 / 1024 / 1024 )) GiB |"
else
  echo "| OS | $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}") |"
  grep -qi microsoft /proc/version 2>/dev/null && echo "| WSL | yes ($(uname -r)) |"
  echo "| CPU | $(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs) / $(nproc) cores |"
  echo "| RAM | $(free -g 2>/dev/null | awk '/Mem:/{print $2}') GiB |"
fi
echo "| Kernel | $(uname -r) |"
echo "| Architecture | $(uname -m) |"
echo "| gcc | $(v gcc --version) |"
echo "| g++ | $(v g++ --version) |"
echo "| clang | $(v clang --version) |"
echo "| cmake | $(v cmake --version) |"
echo "| ninja | $(v ninja --version) |"
echo "| make | $(v make --version) |"
echo "| python3 | $(v python3 --version) |"
echo "| lcov | $(v lcov --version) |"
echo "| genhtml | $(v genhtml --version) |"
echo "| gcov | $(v gcov --version) |"
echo "| git | $(v git --version) |"
if [[ -d "$PX4_DIR/.git" ]]; then
  echo "| PX4 HEAD | $(git -C "$PX4_DIR" rev-parse HEAD) |"
  echo "| PX4 tag at base | $(git -C "$PX4_DIR" describe --tags --always "$(git -C "$PX4_DIR" merge-base HEAD v1.17.0 2>/dev/null || echo HEAD)" 2>/dev/null) |"
  echo "| PX4 branch | $(git -C "$PX4_DIR" branch --show-current) |"
  echo "| merge-base with v1.17.0 | $(git -C "$PX4_DIR" merge-base HEAD 'v1.17.0^{commit}' 2>/dev/null) (expected d6f12ad1c4f70ad3230afd7d86e971421e02fef4) |"
  echo "| Uninitialised submodules | $(git -C "$PX4_DIR" submodule status --recursive 2>/dev/null | grep -c '^-') |"
  if [[ -f "$PX4_DIR/build/px4_sitl_test/CMakeCache.txt" ]]; then
    echo "| build/px4_sitl_test CMAKE_BUILD_TYPE | $(grep -m1 '^CMAKE_BUILD_TYPE' "$PX4_DIR/build/px4_sitl_test/CMakeCache.txt" | cut -d= -f2) |"
    echo "| CMAKE_CXX_COMPILER | $(grep -m1 '^CMAKE_CXX_COMPILER:' "$PX4_DIR/build/px4_sitl_test/CMakeCache.txt" | cut -d= -f2) |"
  fi
else
  echo "| PX4 clone | not found at $PX4_DIR |"
fi
