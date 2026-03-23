#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

GLAD_DIR="$ROOT_DIR/thirdparty/glad"
OUT_DIR="$GLAD_DIR/generated"

echo "Generating glad in $OUT_DIR..."

mkdir -p "$OUT_DIR"

export PYTHONPATH="$GLAD_DIR:$PYTHONPATH"

if ! python3 -c "import jinja2" 2>/dev/null; then
    echo "[ERROR] Failed to generate glad, jinja2 not found"
    exit 1
fi

python3 -m glad --api "gl:compatibility=3.3" --out-path "$OUT_DIR" c

echo "glad generated successfully."