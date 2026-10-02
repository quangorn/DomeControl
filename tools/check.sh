#!/usr/bin/env bash
# The one command that says whether a change is good. AGENTS.md §3 points here and nowhere else;
# nothing else in the repository may repeat these command lines.
#
#   tools/check.sh           every stage this host can run
#   tools/check.sh --quick   L0 + L1 only, about two seconds
#
# Rules for a stage:
#   PASS   the stage ran and every check in it passed
#   FAIL   the stage ran and something failed; the exit code is non-zero
#   SKIP   the stage cannot run here (no avr-gcc, no simavr, no dotnet) -- always with a reason
#
# A SKIP is never printed as a PASS. A stage that cannot run has not verified anything, and an
# agent that mistakes the two will report unverified work as done.

set -uo pipefail

QUICK=0
if [ "${1:-}" = "--quick" ]; then
	QUICK=1
elif [ "$#" -ne 0 ]; then
	echo "usage: $0 [--quick]" >&2
	exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BUILD_ABI="firmware/cmake-build-avr"    # the cross build of the firmware (avr-gcc)
BUILD_TEST="firmware/cmake-build-test"  # the host build of the L1 tests
BUILD_SIM="firmware/cmake-build-sim"    # the simavr harness (firmware/test/sim)
TESTS_CS="ascom/DomeControl.Protocol.Tests"

passed=0
failed=0
skipped=0
declare -a summary=()

start=$(date +%s)

report() {  # report <PASS|FAIL|SKIP> <stage> <detail>
	local status="$1" stage="$2" detail="$3"
	printf '%-5s %-14s %s\n' "$status" "$stage" "$detail"
	case "$status" in
		PASS)  passed=$((passed + 1)) ;;
		FAIL)  failed=$((failed + 1)) ;;
		SKIP)  skipped=$((skipped + 1)) ;;
	esac
	summary+=("$status  $stage  $detail")
}

have() { command -v "$1" > /dev/null 2>&1; }

# ---------------------------------------------------------------- L0, L1: always available
stage_l0_contract() {
	local out
	out=$(python3 tools/test_protocol_contract.py 2>&1)
	local rc=$?
	report "$([ $rc -eq 0 ] && echo PASS || echo FAIL)" "l0-contract" "$(echo "$out" | tail -n 1)"
	[ $rc -ne 0 ] && echo "$out" | sed 's/^/      /'
}

stage_l0_conventions() {
	local out
	out=$(python3 tools/test_conventions.py 2>&1)
	local rc=$?
	report "$([ $rc -eq 0 ] && echo PASS || echo FAIL)" "l0-conventions" "$(echo "$out" | tail -n 1)"
	[ $rc -ne 0 ] && echo "$out" | sed 's/^/      /'
}

stage_l1() {
	if [ ! -f "$BUILD_TEST/CMakeCache.txt" ]; then
		cmake -S firmware/test -B "$BUILD_TEST" > /dev/null 2>&1
	fi
	if ! cmake --build "$BUILD_TEST" > /tmp/domecheck-l1.log 2>&1; then
		report FAIL "l1" "build failed, see /tmp/domecheck-l1.log"
		tail -n 5 /tmp/domecheck-l1.log | sed 's/^/      /'
		return
	fi
	local out rc
	out=$(ctest --test-dir "$BUILD_TEST" 2>&1)
	rc=$?
	local line
	line=$(echo "$out" | grep -E '% tests passed' | tail -n 1)
	if [ -z "$line" ]; then
		report FAIL "l1" "ctest ran no tests"
		echo "$out" | tail -n 5 | sed 's/^/      /'
		return
	fi
	report "$([ $rc -eq 0 ] && echo PASS || echo FAIL)" "l1" "$line"
	[ $rc -ne 0 ] && echo "$out" | grep -E 'Failed|failed' | head -n 10 | sed 's/^/      /'
}

# ------------------------------------------------------------------------- L3: needs simavr
stage_l3() {
	if ! have avr-gcc; then
		report SKIP "l3" "no avr-gcc, run tools/install_deps.sh"
		return
	fi
	if [ ! -f /usr/include/simavr/sim_avr.h ]; then
		report SKIP "l3" "no libsimavr-dev, run tools/install_deps.sh"
		return
	fi

	cmake -S firmware -B "$BUILD_ABI" -DCMAKE_TOOLCHAIN_FILE="$ROOT/firmware/toolchain-avr.cmake" \
		-DCMAKE_BUILD_TYPE=Release > /tmp/domecheck-avr.log 2>&1 &&
		cmake --build "$BUILD_ABI" --config Release >> /tmp/domecheck-avr.log 2>&1
	if [ ! -f "$BUILD_ABI/DomeControl" ]; then
		report FAIL "l3" "firmware build failed, see /tmp/domecheck-avr.log"
		return
	fi
	cmake -S firmware/test/sim -B "$BUILD_SIM" > /dev/null 2>&1 &&
		cmake --build "$BUILD_SIM" >> /tmp/domecheck-sim.log 2>&1
	if [ ! -x "$BUILD_SIM/harness" ]; then
		report FAIL "l3" "harness build failed, see /tmp/domecheck-sim.log"
		return
	fi

	local out rc
	out=$(timeout 600 ctest --test-dir "$BUILD_SIM" --output-on-failure 2>&1)
	rc=$?
	local line
	line=$(echo "$out" | grep -E '% tests passed' | tail -n 1)
	if [ -z "$line" ]; then
		report FAIL "l3" "ctest ran no tests"
		echo "$out" | tail -n 5 | sed 's/^/      /'
		return
	fi
	report "$([ $rc -eq 0 ] && echo PASS || echo FAIL)" "l3" "$line"
	[ $rc -ne 0 ] && echo "$out" | grep -E 'FAIL|not built' | head -n 15 | sed 's/^/      /'
}

# ------------------------------------------------------------------------- C#: needs dotnet
stage_csharp() {
	if ! have dotnet; then
		report SKIP "c#" "no dotnet, run tools/install_deps.sh"
		return
	fi
	if [ ! -f "$TESTS_CS/DomeControl.Protocol.Tests.csproj" ]; then
		report SKIP "c#" "$TESTS_CS does not exist"
		return
	fi
	# Build, then run the produced binary directly. `dotnet run` goes through the CLI's first-run
	# experience, which takes a named mutex and so needs a writable /dev/shm; a sandbox usually
	# denies that, while `dotnet build` on an already-initialised SDK and the test binary itself do
	# not touch it.
	if ! dotnet build "$TESTS_CS" -c Release --nologo > /tmp/domecheck-cs.log 2>&1; then
		report FAIL "c#" "build failed, see /tmp/domecheck-cs.log"
		tail -n 5 /tmp/domecheck-cs.log | sed 's/^/      /'
		return
	fi
	local runner
	runner=$(find "$TESTS_CS/bin/Release" -name DomeControl.Protocol.Tests -type f -perm -u+x | head -n 1)
	if [ -z "$runner" ]; then
		report FAIL "c#" "built, but no test binary under $TESTS_CS/bin/Release"
		return
	fi

	local out rc
	out=$(timeout 600 "$runner" 2>&1)
	rc=$?
	local line
	line=$(echo "$out" | grep -E '^[0-9]+ checks' | tail -n 1)
	if [ -z "$line" ]; then
		report FAIL "c#" "the test binary printed no summary"
		echo "$out" | tail -n 10 | sed 's/^/      /'
		return
	fi
	report "$([ $rc -eq 0 ] && echo PASS || echo FAIL)" "c#" "$line"
	[ $rc -ne 0 ] && echo "$out" | grep -E 'FAIL' | head -n 15 | sed 's/^/      /'
}

stage_l0_contract
stage_l0_conventions
stage_l1
if [ "$QUICK" -eq 0 ]; then
	stage_l3
	stage_csharp
fi

elapsed=$(( $(date +%s) - start ))
echo
printf 'summary (%ds): %d passed, %d failed, %d skipped\n' "$elapsed" "$passed" "$failed" "$skipped"
for line in "${summary[@]}"; do
	echo "  $line"
done
if [ "$skipped" -gt 0 ]; then
	echo
	echo "skipped stages verified nothing; do not report their subject as verified."
fi
echo "not covered by any stage: the ASCOM driver COM layer, and any timing on real hardware."

exit $([ "$failed" -gt 0 ] && echo 1 || echo 0)