#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$LOG_DIR"
exec > >(tee "$LOG_DIR/generate_glad.log") 2>&1

GLAD_DIR="$ROOT_DIR/thirdparty/glad"
OUT_DIR="$GLAD_DIR/generated"

echo "Generating glad in $OUT_DIR..."

export PYTHONPATH="$GLAD_DIR:$PYTHONPATH"

if ! command -v python3 >/dev/null 2>&1; then
	echo "[ERROR] python3 not found"
	exit 1
fi

if ! python3 -c "import jinja2" 2>/dev/null; then
	echo "[ERROR] jinja2 not found"
	exit 1
fi

mkdir -p "$OUT_DIR"
python3 -m glad --api "gl:compatibility=3.3" --out-path "$OUT_DIR" c || {
	echo "[ERROR] Glad generation failed"
	exit 1
}

echo ""
echo "Glad generated"

