#!/usr/bin/env bash
############################################################################
# app/digit/tools/tflm_test.sh
#
# Build digit_test with TFLite Micro for the host and run it, so the int8
# model on TFLite Micro is checked before it goes on the board.  TFLite
# Micro is compiled from the openvela apps tree with the same sources and
# defines as apps/mlearning/tflite-micro/Makefile.  Needs g++.
#
#   tflm_test.sh <apps dir> <mnist dir> [build dir]
############################################################################

set -e

APPS=$(cd "$1" && pwd)
MNIST=$2
OUT=${3:-$(mktemp -d "${TMPDIR:-/tmp}/tflm_test.XXXXXX")}
HERE=$(cd "$(dirname "$0")" && pwd)
TFLM=$APPS/mlearning/tflite-micro/tflite-micro

FLAGS="-O2 -w -I$TFLM -I$APPS/system/flatbuffers/flatbuffers/include \
  -I$APPS/math/gemmlowp/gemmlowp -I$APPS/math/kissfft/kissfft \
  -I$APPS/math/ruy/ruy -I$HERE/.. -DTF_LITE_STRIP_ERROR_STRINGS \
  -DTFLITE_EMULATE_FLOAT -DTF_LITE_DISABLE_X86_NEON"

mkdir -p "$OUT/obj"

for d in c core/c core/api kernels kernels/internal \
         kernels/internal/optimized kernels/internal/reference micro \
         micro/arena_allocator micro/kernels micro/memory_planner \
         micro/tflite_bridge schema; do
  find "$TFLM/tensorflow/lite/$d" -maxdepth 1 -name '*.cc' \
       ! -name '*test.cc'
done > "$OUT/srcs"
echo "$TFLM/tensorflow/lite/array.cc" >> "$OUT/srcs"

# One object per source, named after its path

xargs -P 24 -I{} sh -c \
  'o="$1/obj/$(echo "$2" | tr / _).o"; [ -f "$o" ] || g++ -std=c++17 \
   $3 -c "$2" -o "$o"' _ "$OUT" {} "$FLAGS" < "$OUT/srcs"

# As a library, so only what the interpreter uses gets linked

rm -f "$OUT/libtflm.a"
ar rcs "$OUT/libtflm.a" "$OUT"/obj/*.o

for f in tools/digit_test.c digit.c digit_model.c digit_tflm_model.c; do
  gcc -O2 -I"$HERE/.." -DDIGIT_TEST_TFLM -c "$HERE/../$f" \
      -o "$OUT/$(basename "$f" .c).o"
done

g++ -std=c++17 $FLAGS -o "$OUT/digit_test" "$HERE/../digit_tflm.cc" \
  "$OUT"/digit_test.o "$OUT"/digit.o "$OUT"/digit_model.o \
  "$OUT"/digit_tflm_model.o "$OUT/libtflm.a" -lm

"$OUT/digit_test" "$MNIST"
