#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "1 - Triangle"
echo "2 - Cube"
echo "3 - Shader"
echo "4 - Textured"
echo "5 - Lit"
echo "6 - TexturedLit"
echo "7 - Batch"
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
	*)
		echo "Invalid choice"
		EXAMPLE="Triangle"
		;;
esac

echo "Building example: $EXAMPLE"

BUILD_DIR="$ROOT_DIR/build"
CMAKE_DIR="$BUILD_DIR/cmake"

echo "Cleaning previous build..."
rm -rf "$BUILD_DIR/bin/linux-x86_64" "$BUILD_DIR/obj/linux-x86_64" "$CMAKE_DIR" || true

export BUILD_TYPE=Release

echo "Compiling shaders..."
python3 "$ROOT_DIR/shaders/compile_shaders.py"

mkdir -p "$CMAKE_DIR"
cmake -DEXAMPLE=$EXAMPLE -DBUILD_PS3=OFF -B "$CMAKE_DIR" -S "$ROOT_DIR"

make -j$(nproc) -C "$CMAKE_DIR"

echo "Copying shaders and assets..."
mkdir -p "$BUILD_DIR/bin/linux-x86_64/shaders/glsl"
cp -r "$BUILD_DIR/shaders/glsl/"* "$BUILD_DIR/bin/linux-x86_64/shaders/glsl/" 2>/dev/null || true
mkdir -p "$BUILD_DIR/bin/linux-x86_64/assets"
cp -r "$ROOT_DIR/assets/"* "$BUILD_DIR/bin/linux-x86_64/assets/" 2>/dev/null || true

echo "Build completed"