#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$ROOT_DIR/build"
BIN="$BUILD_DIR/bin/linux-x86_64/gcmgl"

LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$LOG_DIR"
exec > >(tee "$LOG_DIR/run_linux_x86_64.log") 2>&1

if [ ! -x "$BIN" ]; then
	echo "[ERROR] Binary not found: $BIN"
	exit 2
fi

echo "Running $BIN..."
cd "$BUILD_DIR/bin/linux-x86_64"
"$BIN"

echo ""
echo "Run completed"

