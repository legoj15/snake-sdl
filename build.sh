#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./build.sh            # uses "debug"
#   ./build.sh release    # uses "release"
#   PRESET=debug ./build.sh
#   VCPKG_ROOT=... ./build.sh debug

PRESET="${1:-${PRESET:-debug}}"
BUILD_DIR="build"
GAME_DIR="${BUILD_DIR}/game"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "ERROR: missing required command: $1" >&2
    exit 1
  fi
}

require_cmd cmake

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

if [[ -n "${VCPKG_TARGET_TRIPLET:-}" ]]; then
  echo "Using VCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET"
fi

if [[ -n "${VCPKG_ROOT:-}" ]]; then
  VCPKG_EXE="${VCPKG_ROOT}/vcpkg"
  if [[ -x "$VCPKG_EXE" ]]; then
    echo "Installing vcpkg dependencies..."
    if [[ -n "${VCPKG_TARGET_TRIPLET:-}" ]]; then
      "$VCPKG_EXE" install --triplet "$VCPKG_TARGET_TRIPLET" \
        --x-install-root "${BUILD_DIR}/vcpkg_installed"
    else
      "$VCPKG_EXE" install --x-install-root "${BUILD_DIR}/vcpkg_installed"
    fi
  else
    echo "WARN: vcpkg executable not found at $VCPKG_EXE (skipping install)"
  fi
fi

echo "Using preset: $PRESET"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel

rm -rf "$GAME_DIR"
mkdir -p "$GAME_DIR"

PYTHON_BIN="${PYTHON_BIN:-python3}"
if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  PYTHON_BIN="python"
fi
require_cmd "$PYTHON_BIN"

VENV_DIR="launcher/.venv-build"
"$PYTHON_BIN" -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"
python -m pip install --upgrade pip >/dev/null
python -m pip install -r launcher/requirements.txt >/dev/null

echo "Building launcher..."
python -m nuitka --standalone --assume-yes-for-downloads --lto=no --enable-plugin=tk-inter --include-package=customtkinter --output-dir="$BUILD_DIR" --output-filename=launcher launcher/main.py

DIST_DIR="${BUILD_DIR}/main.dist"
if [[ -d "$DIST_DIR" ]]; then
  cp -a "$DIST_DIR"/. "$BUILD_DIR"/
  rm -rf "$DIST_DIR"
fi

if [[ -f "launcher/readme.txt" ]]; then
  cp -a "launcher/readme.txt" "${BUILD_DIR}/readme.txt"
fi

if [[ -f "${BUILD_DIR}/snake" ]]; then
  mv "${BUILD_DIR}/snake" "${GAME_DIR}/snake"
else
  echo "ERROR: missing ${BUILD_DIR}/snake" >&2
  exit 1
fi

echo "Built: ./${BUILD_DIR}/launcher"
echo "Built: ./${GAME_DIR}/snake"
