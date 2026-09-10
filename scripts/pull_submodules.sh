#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$LOG_DIR"
exec > >(tee "$LOG_DIR/pull_submodules.log") 2>&1

PULL_GLAD=0
PULL_SIMDE=0
PULL_OFFSET_ALLOCATOR=0
PULL_SLANG=0
for arg in "$@"; do
	case $arg in
		--pull-glad) PULL_GLAD=1 ;;
		--pull-simde) PULL_SIMDE=1 ;;
		--pull-offset-allocator) PULL_OFFSET_ALLOCATOR=1 ;;
		--pull-slang) PULL_SLANG=1 ;;
	esac
done

if ! command -v git >/dev/null 2>&1; then
	echo "[ERROR] git not found"
	exit 1
fi

echo "Updating submodules..."

MATHSFURY_DIR="$ROOT_DIR/thirdparty/mathsfury"
if [ -d "$MATHSFURY_DIR/.git" ]; then
	echo ""
	echo "Pushing mathsfury submodule..."

	git -C "$MATHSFURY_DIR" push
fi

echo ""
git submodule update --init
git -C "$MATHSFURY_DIR" fetch
git -C "$MATHSFURY_DIR" checkout origin/main
git add "$MATHSFURY_DIR"
echo "mathsfury updated"

if [ "$PULL_GLAD" -eq 1 ]; then
	echo ""
	echo "Updating glad submodule..."

	git -C "$ROOT_DIR/thirdparty/glad" fetch
	git -C "$ROOT_DIR/thirdparty/glad" checkout origin/glad2
	git add "$ROOT_DIR/thirdparty/glad"
	echo "glad updated"
fi

if [ "$PULL_SIMDE" -eq 1 ]; then
	echo ""
	echo "Updating simde submodule..."

	git -C "$ROOT_DIR/thirdparty/simde" fetch
	git -C "$ROOT_DIR/thirdparty/simde" checkout origin/master
	git add "$ROOT_DIR/thirdparty/simde"
	echo "simde updated"
fi

if [ "$PULL_OFFSET_ALLOCATOR" -eq 1 ]; then
	echo ""
	echo "Updating OffsetAllocator submodule..."

	git -C "$ROOT_DIR/thirdparty/OffsetAllocator" fetch
	git -C "$ROOT_DIR/thirdparty/OffsetAllocator" checkout origin/main
	git add "$ROOT_DIR/thirdparty/OffsetAllocator"
	echo "OffsetAllocator updated"
fi

if [ "$PULL_SLANG" -eq 1 ]; then
	echo ""
	echo "Updating slang submodule..."

	git -C "$ROOT_DIR/thirdparty/slang" fetch
	git -C "$ROOT_DIR/thirdparty/slang" checkout origin/master
	git add "$ROOT_DIR/thirdparty/slang"
	echo "slang updated"
fi

echo ""
echo "Committing updated submodules..."

git commit -m "feat: update submodules" || {
	echo "[INFO] No submodule changes to commit"
}

echo ""
echo "Submodule update complete"

