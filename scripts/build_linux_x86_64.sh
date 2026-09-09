#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$LOG_DIR"
exec > >(tee "$LOG_DIR/build_linux_x86_64.log") 2>&1

echo "1 - Triangle"
echo "2 - Cube"
echo "3 - Shader"
echo "4 - Textured"
echo "5 - Lit"
echo "6 - TexturedLit"
echo "7 - Batch"
echo "8 - BatchInstanced"
read -p "Select an example to build: " choice
choice=${choice:-1}

case $choice in
	1)
		EXAMPLE="Triangle"
		;;
	2)
		EXAMPLE="Cube"
		;;
	3)
		EXAMPLE="Shader"
		;;
	4)
		EXAMPLE="Textured"
		;;
	5)
		EXAMPLE="Lit"
		;;
	6)
		EXAMPLE="TexturedLit"
		;;
	7)
		EXAMPLE="Batch"
		;;
	8)
		EXAMPLE="BatchInstanced"
		;;
	*)
		echo "[WARNING] Invalid choice, defaulting to Triangle"
		EXAMPLE="Triangle"
		;;
esac

echo ""
echo "[INFO] Building example: $EXAMPLE"

BUILD_DIR="$ROOT_DIR/build"
CMAKE_DIR="$BUILD_DIR/cmake"
export BUILD_TYPE=Release

if ! command -v cmake >/dev/null 2>&1; then
	echo "[ERROR] cmake not found"
	exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
	echo "[ERROR] python3 not found"
	exit 1
fi

echo ""
echo "Cleaning previous build..."
rm -rf "$BUILD_DIR/bin/linux-x86_64" "$BUILD_DIR/obj/linux-x86_64" "$CMAKE_DIR" || true
echo "Previous build cleaned"

echo ""
echo "Compiling shaders..."
python3 "$ROOT_DIR/shaders/compile_shaders.py" || {
	echo "[ERROR] Shader compilation failed"
	exit 1
}
echo "Shaders compiled"

echo ""
echo "Configuring build..."
mkdir -p "$CMAKE_DIR"
cmake "-DEXAMPLE=$EXAMPLE" -DBUILD_PS3=OFF -B "$CMAKE_DIR" -S "$ROOT_DIR" || {
	echo "[ERROR] CMake configuration failed"
	exit 1
}
echo "Build configured"

echo ""
echo "Compiling..."
make -j"$(nproc)" -C "$CMAKE_DIR" || {
	echo "[ERROR] Compilation failed"
	exit 1
}
echo "Compilation complete"

echo ""
echo "Copying shaders and assets..."
mkdir -p "$BUILD_DIR/bin/linux-x86_64/shaders/glsl"
cp -r "$BUILD_DIR/shaders/glsl/"* "$BUILD_DIR/bin/linux-x86_64/shaders/glsl/" 2>/dev/null || true
mkdir -p "$BUILD_DIR/bin/linux-x86_64/assets"
cp -r "$ROOT_DIR/assets/"* "$BUILD_DIR/bin/linux-x86_64/assets/" 2>/dev/null || true
echo "Assets copied"

echo ""
echo "Build completed"

