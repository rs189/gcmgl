#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

echo "Updating mathsfury submodule..."
git submodule update --init --recursive --remote

echo "Committing updated submodules..."
git add src/mathsfury thirdparty/glad thirdparty/simde

git commit -m "feat: update submodules" || true

echo "Submodules updated successfully"