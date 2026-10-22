#!/bin/bash
# Profile-guided, LTO build of liblexbor trained on the resiliparse workload.
#
# usage: build_pgo.sh <lexbor-src> <build-dir> <install-prefix> <training-command...>
#
# The training command must run resiliparse with the instrumented library,
# e.g. with RESILIPARSE_LIBS pointing at the wheel's resiliparse.libs:
#   build_pgo.sh . /tmp/b /tmp/i sh -c 'cp /tmp/i/lib/liblexbor.so.2.4.0 \
#       $RESILIPARSE_LIBS/liblexbor-*.so.2.4.0 && python bench.py train.raw.bin --reps 1'
# Train on documents other than the ones you benchmark.
set -e
src=$1 build=$2 prefix=$3; shift 3
flags="-fno-semantic-interposition -flto=auto"
rm -rf "$build"
cmake -S "$src" -B "$build" -DCMAKE_INSTALL_PREFIX="$prefix" -DLEXBOR_BUILD_SHARED=ON \
    -DLEXBOR_BUILD_STATIC=OFF -DLEXBOR_OPTIMIZATION_LEVEL=-O3 \
    -DCMAKE_C_FLAGS="$flags -fprofile-generate -fprofile-update=single" >/dev/null
cmake --build "$build" -j"$(nproc)" >/dev/null
cmake --install "$build" >/dev/null
"$@"
cmake -S "$src" -B "$build" \
    -DCMAKE_C_FLAGS="$flags -fprofile-use -fprofile-partial-training -Wno-missing-profile" >/dev/null
cmake --build "$build" -j"$(nproc)" >/dev/null
cmake --install "$build" >/dev/null
echo "installed $prefix/lib/liblexbor.so.2.4.0"
