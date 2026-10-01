# P01 — Environment setup + fixed baseline clone
**Goal:** a supported local toolchain and a verified clone of v1.17.0. **Owner:** ENV. **Gate:** G01. **Rubric:** Part 1 (setup evidence).

## Entry
G00 approved. ≥ 25 GB free disk (recursive clone + coverage build), ≥ 8 GB RAM recommended (4 GB works with `-j2`).

## Step 1 — Platform prerequisites
**Ubuntu 22.04 / 24.04 (native):** `sudo apt-get update && sudo apt-get install -y git`
**Windows → WSL2:** in PowerShell (admin): `wsl --install -d Ubuntu-24.04`; reboot; open Ubuntu. Work **inside the Linux filesystem**
(`~/sqe-a2`), never under `/mnt/c` (slow I/O, permission and line-ending problems). Optionally create `%UserProfile%\.wslconfig`:
```
[wsl2]
memory=8GB
processors=4
swap=8GB
```
then `wsl --shutdown` and reopen. Install git as above.
**macOS (Intel or Apple Silicon):** install Xcode Command Line Tools (`xcode-select --install`) and Homebrew. PX4's script installs the rest.

## Step 2 — Clone the fixed baseline (exact command from the assignment)
```bash
cd ~/sqe-a2
git clone --branch v1.17.0 --recursive https://github.com/PX4/PX4-Autopilot.git
cd PX4-Autopilot
git rev-parse HEAD                       # MUST print d6f12ad1c4f70ad3230afd7d86e971421e02fef4
git rev-parse 'v1.17.0^{commit}'         # same commit
git rev-parse v1.17.0                    # prints the TAG OBJECT a5eb12d2ab591251faa009f76b2685b8cc64405d (do not report this as the commit)
git describe --tags --exact-match        # v1.17.0
git status --porcelain | wc -l           # 0
git submodule status --recursive | grep -c '^-' || true   # uninitialised submodules; expect 0
git switch -c sqe-a2                     # all student work happens on this branch
```
Save the output: `{ date; git rev-parse HEAD; git describe --tags --exact-match; git log -1 --format='%H %ad %s' --date=iso; } | tee ../evidence/env/baseline_commit.txt`
If a submodule failed (network), run `git submodule update --init --recursive` and re-check.

## Step 3 — PX4 toolchain (official scripts inside the clone)
**Ubuntu / WSL2:** `bash ./Tools/setup/ubuntu.sh --no-nuttx --no-sim-tools` (supports 24.04 and 22.04; installs gcc/g++, cmake,
ninja-build, lcov, python deps; on Python ≥ 3.11 it pip-installs with `--break-system-packages`). Log out/in afterwards.
NuttX toolchain and simulators are not needed for this assignment (`--no-nuttx --no-sim-tools`). If you also want QGroundControl/Gazebo later, rerun without flags.
**macOS:** `bash ./Tools/setup/macos.sh` (Homebrew `px4-dev`, Python requirements). Then `brew install lcov` if `lcov` is missing.
Record the exact script command in `work/DECISIONS.md` (D-002).

## Step 4 — macOS/Clang only: coverage flag fix (tooling, not production)
`cmake/coverage.cmake` sets `CMAKE_CXX_FLAGS_COVERAGE` for Clang to `"... -O0-fprofile-arcs ..."` (missing space; FORCE-cached).
If the P03 coverage build fails with an error mentioning `-O0-fprofile-arcs`, apply:
```bash
sed -i '' 's/-O0-fprofile-arcs/-O0 -fprofile-arcs/' cmake/coverage.cmake     # macOS sed
rm -rf build/px4_sitl_test                                                   # cache holds the FORCE value
```
Log it in `work/production_change_log.md` as `TOOLING-01: build-flag typo fix, no behavioural change` and commit it separately.
Also create the gcov wrapper for lcov: `tools/llvm-gcov.sh` (already in the kit) and later export `GCOV_TOOL=$PWD/../tools/llvm-gcov.sh`.

## Step 5 — Environment evidence
`bash ../tools/sqe_env_report.sh > ../evidence/env/environment.md` — OS/version, kernel, CPU arch/count, RAM, gcc/g++/clang, cmake, ninja,
python3, lcov/genhtml/gcov versions, git version, HEAD, branch. Paste the table into the report later (SPEC_06 §2).

## Gate G01 checklist
- [ ] HEAD = d6f12ad1c4f70ad3230afd7d86e971421e02fef4, branch `sqe-a2`, clean tree
- [ ] `evidence/env/environment.md` complete (no "not found" for gcc/cmake/ninja/lcov/gcov/python3)
- [ ] D-001/D-002 recorded; macOS tooling fix logged if applied
## Explain-back (work/explain/P01.md)
Tag vs commit hash (why `git rev-parse v1.17.0` differs); why recursive clone; why WSL files must live in Linux FS; what the setup script installed.
