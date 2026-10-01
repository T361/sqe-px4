#!/bin/sh
# macOS: lcov needs a gcov-compatible tool for Clang coverage data. Use: GCOV_TOOL="$PWD/tools/llvm-gcov.sh"
if command -v xcrun >/dev/null 2>&1; then exec xcrun llvm-cov gcov "$@"; fi
exec llvm-cov gcov "$@"
