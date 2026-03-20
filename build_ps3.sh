#!/bin/bash
set -e

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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/bin/ps3"

echo "Cleaning previous build..."
rm -rf build/bin/ps3/ build/obj/ps3/ || true

export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
export EXAMPLE=$EXAMPLE
export BUILD_TYPE=Release

echo "Compiling shaders..."
python3 "$ROOT_DIR/shaders/compile_shaders.py"

make -f Makefile.ps3

if [ -f "build/bin/ps3/gcmgl.self" ]; then
	rm -rf "$BUILD_DIR/bin/ps3/shaders" "$BUILD_DIR/bin/ps3/assets" 2>/dev/null || true

	mkdir -p "$BUILD_DIR/bin/ps3/USRDIR/shaders/cg"
	mkdir -p "$BUILD_DIR/bin/ps3/USRDIR/assets"

	cp -r "$ROOT_DIR/build/shaders/cg/"* "$BUILD_DIR/bin/ps3/USRDIR/shaders/cg/" 2>/dev/null || true
	cp -r "$ROOT_DIR/assets/"* "$BUILD_DIR/bin/ps3/USRDIR/assets/" 2>/dev/null || true

	cp "$BUILD_DIR/bin/ps3/gcmgl.self" "$BUILD_DIR/bin/ps3/USRDIR/EBOOT.BIN"
	cp "$BUILD_DIR/bin/ps3/gcmgl.elf" "$BUILD_DIR/bin/ps3/USRDIR/" 2>/dev/null || true

	if [ -f "$ROOT_DIR/PARAM.SFO.xml" ]; then
		if command -v python3 >/dev/null 2>&1 && [ -f "/usr/local/ps3dev/bin/sfo.py" ]; then
			python3 "/usr/local/ps3dev/bin/sfo.py" --fromxml "$ROOT_DIR/PARAM.SFO.xml" "$BUILD_DIR/bin/ps3/PARAM.SFO" || {
				echo "Build failed, sfo.py failed"
				exit 1
			}
		else
			echo "Build failed, sfo.py not available"
			exit 1
		fi
	fi

	if command -v convert >/dev/null 2>&1; then
		convert -size 320x176 xc:blue -fill white -gravity center -pointsize 24 -annotate +0+0 "gcmgl" "$BUILD_DIR/bin/ps3/ICON0.PNG" || {
			touch "$BUILD_DIR/bin/ps3/ICON0.PNG"
		}
	else
		touch "$BUILD_DIR/bin/ps3/ICON0.PNG"
	fi

	mkdir -p "$BUILD_DIR/obj/ps3"
	mv "$BUILD_DIR/bin/ps3/gcmgl.elf" "$BUILD_DIR/bin/ps3/gcmgl.elf.map" "$BUILD_DIR/bin/ps3/gcmgl.fake.self" "$BUILD_DIR/obj/ps3/" 2>/dev/null || true
else
	echo "Build failed"
	exit 1
fi

echo "Build completed"