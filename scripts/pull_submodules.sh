#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

PULL_GLAD=0
PULL_SIMDE=0
for arg in "$@"; do
	case $arg in
		--pull-glad)  PULL_GLAD=1 ;;
		--pull-simde) PULL_SIMDE=1 ;;
	esac
done

echo "Updating submodules..."

MATHSFURY_DIR="$(cd "$ROOT_DIR/../mathsfury" && pwd)"
if [ -d "$MATHSFURY_DIR/.git" ]; then
	echo "Pushing mathsfury submodule..."

	git -C "$MATHSFURY_DIR" push
fi

git submodule update --init
git -C src/mathsfury fetch
git -C src/mathsfury checkout origin/main
git add src/mathsfury

if [ "$PULL_GLAD" -eq 1 ]; then
	echo "Updating glad submodule..."

	git -C thirdparty/glad fetch
	git -C thirdparty/glad checkout origin/glad2
	git add thirdparty/glad
fi

if [ "$PULL_SIMDE" -eq 1 ]; then
	echo "Updating simde submodule..."

	git -C thirdparty/simde fetch
	git -C thirdparty/simde checkout origin/master
	git add thirdparty/simde
fi

echo "Committing updated submodules..."

git commit -m "feat: update submodules" || true

echo "Submodules updated successfully"