#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./build.sh            # uses "debug"
#   ./build.sh release    # uses "release"
#   PRESET=debug ./build.sh
#   VCPKG_ROOT=... ./build.sh debug

rm -rf build

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
  if command -v vcpkg >/dev/null 2>&1; then
    VCPKG_EXE="$(command -v vcpkg)"
    VCPKG_ROOT="$(dirname "$VCPKG_EXE")"
    TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  else
  cat >&2 <<'EOF'
ERROR: vcpkg not configured.

Set one of:
  - VCPKG_ROOT=/path/to/vcpkg
  - VCPKG_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
EOF
  exit 1
  fi
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
LAUNCHER_OUT_DIR="${BUILD_DIR}/launcher_build"
python -m nuitka --standalone --assume-yes-for-downloads --lto=no --enable-plugin=tk-inter --include-package=customtkinter --output-dir="$LAUNCHER_OUT_DIR" --output-filename=launcher launcher/main.py

DIST_DIR="${LAUNCHER_OUT_DIR}/main.dist"
if [[ -d "$DIST_DIR" ]]; then
  cp -a "$DIST_DIR"/. "$BUILD_DIR"/
  rm -rf "$LAUNCHER_OUT_DIR"
fi

if [[ -f "launcher/readme.txt" ]]; then
  cp -a "launcher/readme.txt" "${BUILD_DIR}/readme.txt"
fi

find_snake() {
  local candidate
  for candidate in \
    "${BUILD_DIR}/snake" \
    "${BUILD_DIR}/Debug/snake" \
    "${BUILD_DIR}/Release/snake" \
    "${BUILD_DIR}/bin/snake"; do
    if [[ -f "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  candidate="$(find "${BUILD_DIR}" -maxdepth 3 -type f -name snake -perm -u+x 2>/dev/null | head -n 1)"
  if [[ -n "$candidate" ]]; then
    echo "$candidate"
    return 0
  fi
  return 1
}

SNAKE_PATH="$(find_snake || true)"
if [[ -z "$SNAKE_PATH" ]]; then
  echo "Snake binary missing; retrying explicit build target..." >&2
  cmake --build --preset "$PRESET" --parallel --target snake
  SNAKE_PATH="$(find_snake || true)"
fi

if [[ -n "$SNAKE_PATH" ]]; then
  mv "$SNAKE_PATH" "${GAME_DIR}/snake"
else
  echo "ERROR: missing snake binary after build." >&2
  exit 1
fi

if [[ -d "assets" ]]; then
  echo "Copying game assets..."
  mkdir -p "${GAME_DIR}/assets"
  cp -a "assets/." "${GAME_DIR}/assets/"
else
  echo "WARN: assets/ not found; skipping asset copy."
fi

if [[ -f "${BUILD_DIR}/launcher" ]]; then
  chmod +x "${BUILD_DIR}/launcher"
fi
if [[ -f "${GAME_DIR}/snake" ]]; then
  chmod +x "${GAME_DIR}/snake"
fi

echo "Built: ./${BUILD_DIR}/launcher"
echo "Built: ./${GAME_DIR}/snake"
