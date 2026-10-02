#!/usr/bin/env bash
# Install the host packages the DomeControl checks need. Run once; tools/check.sh then does
# everything else. Idempotent: a package whose command is already present is not installed again.
#
#   tools/install_deps.sh            install what is missing
#   tools/install_deps.sh --dry-run  print the plan and exit
#
# This is the only place in the project that needs network access and root: the C# tests
# deliberately have no NuGet dependencies, and tools/check.sh never downloads anything.

set -euo pipefail

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
	DRY_RUN=1
elif [ "$#" -ne 0 ]; then
	echo "usage: $0 [--dry-run]" >&2
	exit 2
fi

# package : what it provides (a command, or an absolute path) : why DomeControl needs it
PACKAGES=(
	"gcc-avr:avr-gcc:cross-compiles the firmware for atmega8 (AGENTS.md §3)"
	"avr-libc:/usr/lib/avr/libc/avr5/libc.a:C runtime for the target"
	"binutils-avr:avr-size:firmware size report and .elf -> .hex"
	"simavr:simavr:AVR simulator CLI"
	"libsimavr-dev:/usr/include/simavr/sim_avr.h:headers and libsimavrparts for firmware/test/sim/harness.c"
	"dotnet-sdk-8.0:dotnet:builds and tests ascom/DomeControl.Protocol on Linux"
)

missing=()
for entry in "${PACKAGES[@]}"; do
	package="${entry%%:*}"
	rest="${entry#*:}"
	provides="${rest%%:*}"
	reason="${rest#*:}"
	case "$provides" in
		/*) present=0; [ -e "$provides" ] && present=1 ;;
		*)  present=0; command -v "$provides" > /dev/null 2>&1 && present=1 ;;
	esac
	if [ "$present" -eq 1 ]; then
		echo "ok       $package (${provides})"
	else
		echo "missing  $package (${provides}) - ${reason}"
		missing+=("$package")
	fi
done

if [ "${#missing[@]}" -eq 0 ]; then
	echo "nothing to install"
	exit 0
fi

if [ "$DRY_RUN" -eq 1 ]; then
	echo
	echo "would install: ${missing[*]}"
	exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
	if ! command -v sudo > /dev/null 2>&1; then
		echo "need root or sudo to install: ${missing[*]}" >&2
		exit 1
	fi
	echo
	echo "installing: ${missing[*]}"
	sudo apt-get update -qq
	sudo apt-get install -y "${missing[@]}"
else
	echo
	echo "installing: ${missing[*]}"
	apt-get update -qq
	apt-get install -y "${missing[@]}"
fi

echo
echo "done; now run tools/check.sh"