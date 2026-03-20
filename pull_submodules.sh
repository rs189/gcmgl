#!/bin/bash
set -e

echo "Updating mathsfury submodule..."
git submodule update --init --recursive --remote

echo "Committing updated submodules..."
git add src/mathsfury thirdparty/glad thirdparty/simde

git commit -m "feat: update submodules" || true

echo "Submodules updated successfully"