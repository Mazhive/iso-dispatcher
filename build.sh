#!/usr/bin/env bash
set -euo pipefail

# Build iso-dispatcher in the project directory.
# Usage:
#   ./build.sh            # configure + build
#   ./build.sh clean      # remove local build output and temp build dir
#   ./build.sh --tmp DIR  # use a specific temp build dir

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR"
OUT_DIR="$SRC_DIR/build"
TMP_BUILD="${TMPDIR:-/tmp}/iso-dispatcher-build"
GENERATOR="Ninja"

if [[ "${1:-}" == "clean" ]]; then
  rm -rf "$OUT_DIR" "$TMP_BUILD"
  echo "Cleaned: $OUT_DIR and $TMP_BUILD"
  exit 0
fi

if [[ "${1:-}" == "--tmp" ]]; then
  shift
  TMP_BUILD="${1:?missing DIR}"
  shift || true
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake not found" >&2
  exit 1
fi
if ! command -v g++ >/dev/null 2>&1; then
  echo "ERROR: g++ not found" >&2
  exit 1
fi
if ! command -v ninja >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
fi

# Sanity check Qt6 pkg-config/cmake presence (non-fatal here; cmake will report clearly).
if ! pkg-config --exists Qt6Core Qt6Widgets 2>/dev/null; then
  echo "WARNING: pkg-config cannot find Qt6Core/Qt6Widgets. CMake may still find Qt6 via CMake packages." >&2
fi

mkdir -p "$OUT_DIR" "$(dirname "$TMP_BUILD")"

echo "==> Configuring"
cmake -S "$SRC_DIR" -B "$TMP_BUILD" -G "$GENERATOR"

echo "==> Building"
cmake --build "$TMP_BUILD" -j

BIN="$TMP_BUILD/iso-dispatcher"
if [[ ! -f "$BIN" ]]; then
  echo "ERROR: build output not found: $BIN" >&2
  exit 1
fi

echo "==> Installing binary to $OUT_DIR/iso-dispatcher"
cp -f "$BIN" "$OUT_DIR/iso-dispatcher"
chmod +x "$OUT_DIR/iso-dispatcher"

echo "==> Done"
ls -lh "$OUT_DIR/iso-dispatcher"
"$OUT_DIR/iso-dispatcher" --help | head -1
