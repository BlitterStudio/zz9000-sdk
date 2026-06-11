#!/bin/sh
#
# Build amissl.library with the ZZ9000 crypto-offload provider compiled in, so
# every application that opens the library gets accelerated TLS with no changes
# of its own (Path A — see README.md).
#
# Run this inside an AmiSSL m68k build environment. The
# sacredbanana/amiga-compiler:m68k-amigaos image has everything needed
# (m68k-amigaos-gcc, sfdc, clib2, make). Example:
#
#   docker run --rm \
#     -v "$PWD:/sdk" -v "$PWD/../amissl:/amissl" \
#     sacredbanana/amiga-compiler:m68k-amigaos \
#     sh -c 'AMISSL_DIR=/amissl ZZ9000_SDK=/sdk /sdk/integration/amissl/build.sh'
#
# Environment overrides:
#   ZZ9000_SDK   path to the zz9000-sdk checkout      (default: repo root)
#   AMISSL_DIR   use an existing AmiSSL checkout       (default: clone into work/)
#   AMISSL_REPO  git URL to clone when AMISSL_DIR unset
#   OS           AmiSSL build target                   (default: os3-68020 = m68k)
#   WORK         scratch directory                     (default: integration/amissl/work)
#
# SPDX-License-Identifier: GPL-3.0-or-later
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ZZ9000_SDK=${ZZ9000_SDK:-$(cd "$HERE/../.." && pwd)}
AMISSL_REF=$(cat "$HERE/AMISSL_REF")
AMISSL_REPO=${AMISSL_REPO:-https://github.com/jens-maus/amissl.git}
OS=${OS:-os3-68020}
WORK=${WORK:-$HERE/work}
PATCH="$HERE/amissl-zz9000.patch"

mkdir -p "$WORK"

if [ -n "$AMISSL_DIR" ]; then
  SRC="$AMISSL_DIR"
  echo ">> Using existing AmiSSL checkout: $SRC"
else
  SRC="$WORK/amissl"
  if [ ! -d "$SRC/.git" ]; then
    echo ">> Cloning AmiSSL into $SRC"
    git clone "$AMISSL_REPO" "$SRC"
  fi
  echo ">> Checking out pinned ref $AMISSL_REF"
  git -C "$SRC" fetch --tags origin 2>/dev/null || true
  git -C "$SRC" checkout -f "$AMISSL_REF"
fi

# Apply the integration patch idempotently (revert the two touched files first).
echo ">> Applying integration patch"
git -C "$SRC" checkout -- Makefile src/amissl_library.c 2>/dev/null || true
git -C "$SRC" apply "$PATCH"

echo ">> Building amissl.library (OS=$OS, ZZ9000_SDK=$ZZ9000_SDK)"
make -C "$SRC" OS="$OS" ZZ9000_SDK="$ZZ9000_SDK"

mkdir -p "$WORK/out"
echo ">> Collecting built libraries"
find "$SRC" -name 'amissl*.library' -newer "$PATCH" -exec cp -v {} "$WORK/out/" \; || true
echo ">> Done. Output in $WORK/out/"
ls -l "$WORK/out/" 2>/dev/null || true
