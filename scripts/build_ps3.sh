#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$LOG_DIR"
exec > >(tee "$LOG_DIR/build_ps3.log") 2>&1

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

export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
export EXAMPLE=$EXAMPLE
export BUILD_TYPE=Release

if ! command -v python3 >/dev/null 2>&1; then
	echo "[ERROR] python3 not found"
	exit 1
fi

echo ""
echo "Cleaning previous build..."
rm -rf "$BUILD_DIR/bin/ps3" "$BUILD_DIR/obj/ps3" || true
echo "Previous build cleaned"

echo ""
echo "Compiling shaders..."
python3 "$ROOT_DIR/shaders/compile_shaders.py" || {
	echo "[ERROR] Shader compilation failed"
	exit 1
}
echo "Shaders compiled"

echo ""
echo "Building PS3 target..."
make -C "$ROOT_DIR" -f "$ROOT_DIR/Makefile.ps3" || {
	echo "[ERROR] PS3 build failed"
	exit 1
}
echo "PS3 build finished"

if [ ! -f "$BUILD_DIR/bin/ps3/gcmgl.self" ]; then
	echo "[ERROR] gcmgl.self not produced, build failed"
	exit 1
fi

echo ""
echo "Packaging USRDIR..."
rm -rf "$BUILD_DIR/bin/ps3/shaders" "$BUILD_DIR/bin/ps3/assets" 2>/dev/null || true

mkdir -p "$BUILD_DIR/bin/ps3/USRDIR/shaders/cg"
mkdir -p "$BUILD_DIR/bin/ps3/USRDIR/assets"

cp -r "$BUILD_DIR/shaders/cg/"* "$BUILD_DIR/bin/ps3/USRDIR/shaders/cg/" 2>/dev/null || true
cp -r "$ROOT_DIR/assets/"* "$BUILD_DIR/bin/ps3/USRDIR/assets/" 2>/dev/null || true

cp "$BUILD_DIR/bin/ps3/gcmgl.self" "$BUILD_DIR/bin/ps3/USRDIR/EBOOT.BIN"

if [ -f "$ROOT_DIR/PARAM.SFO.xml" ]; then
	if [ -f "$PS3DEV/bin/sfo.py" ]; then
		python3 "$PS3DEV/bin/sfo.py" --fromxml "$ROOT_DIR/PARAM.SFO.xml" "$BUILD_DIR/bin/ps3/PARAM.SFO" || {
			echo "[ERROR] sfo.py failed, PARAM.SFO not generated"
			exit 1
		}

		echo "PARAM.SFO generated"
	else
		echo "[ERROR] sfo.py not available, PARAM.SFO not generated"
		exit 1
	fi
else
	echo "[INFO] No PARAM.SFO.xml found, skipping SFO generation"
fi

if command -v convert >/dev/null 2>&1; then
	convert -size 320x176 xc:blue -fill white -gravity center -pointsize 24 -annotate +0+0 "gcmgl" "$BUILD_DIR/bin/ps3/ICON0.PNG" || {
		echo "[WARNING] Icon generation failed, writing placeholder ICON0.PNG"
		touch "$BUILD_DIR/bin/ps3/ICON0.PNG"
	}

	echo "ICON0.PNG written"
else
	echo "[WARNING] convert not found, writing placeholder ICON0.PNG"
	touch "$BUILD_DIR/bin/ps3/ICON0.PNG"
fi

mkdir -p "$BUILD_DIR/obj/ps3"
mv "$BUILD_DIR/bin/ps3/gcmgl.elf" "$BUILD_DIR/bin/ps3/gcmgl.elf.map" "$BUILD_DIR/bin/ps3/gcmgl.fake.self" "$BUILD_DIR/obj/ps3/" 2>/dev/null || true

echo "Package ready"

echo ""
echo "Build completed"

