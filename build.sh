#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./build.sh            # uses "debug"
#   ./build.sh release    # uses "release"
#   PRESET=debug ./build.sh
#   VCPKG_ROOT=... ./build.sh debug

PRESET="${1:-${PRESET:-debug}}"

# vcpkg toolchain resolution (same as before)
if [[ -n "${VCPKG_TOOLCHAIN_FILE:-}" ]]; then
  TOOLCHAIN="$VCPKG_TOOLCHAIN_FILE"
elif [[ -n "${VCPKG_ROOT:-}" ]]; then
  TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
  cat >&2 <<'EOF'
ERROR: vcpkg not configured.

Set one of:
  - VCPKG_ROOT=/path/to/vcpkg
  - VCPKG_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
EOF
  exit 1
fi

if [[ ! -f "$TOOLCHAIN" ]]; then
  echo "ERROR: vcpkg toolchain file not found: $TOOLCHAIN" >&2
  exit 1
fi

# Optional triplet support (your preset must read this env var to use it)
# Example: VCPKG_TARGET_TRIPLET=x64-linux ./build.sh debug
if [[ -n "${VCPKG_TARGET_TRIPLET:-}" ]]; then
  echo "Using VCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET"
fi

echo "Using preset: $PRESET"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel

# Best-effort "Built:" message (depends on your preset's binaryDir)
echo "Built (if binaryDir is default): ./build/snake"
