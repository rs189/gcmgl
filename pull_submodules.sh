#!/bin/bash
set -e

echo "Updating mathsfury submodule..."

cd src/mathsfury
git fetch origin
git reset --hard origin/main

cd ../..

echo "Committing updated mathsfury submodule..."
git add src/mathsfury
git commit -m "Update mathsfury submodule after force push" || true

echo "mathsfury submodule updated successfully"