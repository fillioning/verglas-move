#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_DIR/src"
BUILD_DIR="$PROJECT_DIR/build"
OUT_DIR="$PROJECT_DIR/modules/verglas"

# Cross-compiler prefix (Docker or local toolchain)
CC="${CROSS_PREFIX:-aarch64-linux-gnu-}g++"

echo "=== Building Verglas for Ableton Move (ARM64) ==="
echo "Compiler: $CC"

mkdir -p "$BUILD_DIR" "$OUT_DIR"

# Source files
SOURCES=(
    "$SRC_DIR/clouds_move.cpp"
    "$SRC_DIR/clouds/dsp/granular_processor.cc"
    "$SRC_DIR/clouds/dsp/correlator.cc"
    "$SRC_DIR/clouds/dsp/mu_law.cc"
    "$SRC_DIR/clouds/resources.cc"
    "$SRC_DIR/clouds/dsp/pvoc/phase_vocoder.cc"
    "$SRC_DIR/clouds/dsp/pvoc/stft.cc"
    "$SRC_DIR/clouds/dsp/pvoc/frame_transformation.cc"
    "$SRC_DIR/stmlib/dsp/units.cc"
    "$SRC_DIR/stmlib/dsp/atan.cc"
)

# Compile each source
OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="$BUILD_DIR/$(basename "$src" | sed 's/\.\(cpp\|cc\|c\)$/.o/')"
    echo "  CC $src"
    $CC -c "$src" -o "$obj" \
        -I"$SRC_DIR" \
        -std=c++17 \
        -O2 -fPIC -ffast-math \
        -fno-exceptions -fno-rtti \
        -DTEST \
        -Wall -Wno-unused-variable -Wno-unused-but-set-variable
    OBJECTS+=("$obj")
done

# Link shared library
echo "  LD verglas.so"
$CC -shared -o "$OUT_DIR/verglas.so" "${OBJECTS[@]}" -lm

# Copy module files
cp "$PROJECT_DIR/module.json" "$OUT_DIR/"
cp "$PROJECT_DIR/ui_chain.js" "$OUT_DIR/"
if [ -f "$SRC_DIR/help.json" ]; then
    cp "$SRC_DIR/help.json" "$OUT_DIR/"
fi

echo "=== Build complete: $OUT_DIR/verglas.so ==="
ls -la "$OUT_DIR/"
