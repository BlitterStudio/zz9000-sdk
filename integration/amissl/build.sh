#!/bin/sh
#
# Build amissl.library with the ZZ9000 crypto-offload provider compiled in, so
# every application that opens the library gets accelerated TLS with no changes
# of its own (Path A — see README.md).
#
# Needs the adtools m68k build environment AmiSSL's own CI uses; the supported
# way to run it is the GitHub workflow (.github/workflows/build-amissl-provider.yml)
# or an equivalent ubuntu container. See README.md ("Building") for the
# environment details and local-build caveats.
#
# Environment overrides:
#   ZZ9000_SDK   path to the zz9000-sdk checkout      (default: repo root)
#   AMISSL_DIR   use an existing AmiSSL checkout       (default: clone into work/)
#   AMISSL_REPO  git URL to clone when AMISSL_DIR unset
#   OS           AmiSSL build target                   (default: os3-68020)
#   DEBUG        AmiSSL DEBUG flags; empty = release   (default: empty)
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

# AmiSSL's Makefile hard-assigns DEBUG (-DDEBUG -g -gstabs), so a release
# build needs the override on the make command line, exactly like AmiSSL's
# own release targets (`make OS=... DEBUG=`).
echo ">> Building amissl.library (OS=$OS, DEBUG='${DEBUG-}', ZZ9000_SDK=$ZZ9000_SDK)"
make -C "$SRC" OS="$OS" ZZ9000_SDK="$ZZ9000_SDK" DEBUG="${DEBUG-}"

# Collect the build products deterministically and fail loudly if the expected
# library is missing — a green run must mean a usable artifact.
OUT="$WORK/out/$OS"
mkdir -p "$OUT"
echo ">> Collecting built libraries into $OUT"
FOUND=0
for lib in "$SRC/build_$OS"/amissl_v*.library; do
  [ -f "$lib" ] || continue
  cp -v "$lib" "$OUT/"
  FOUND=1
done
if [ "$FOUND" != 1 ]; then
  echo "!! No amissl_v*.library produced in $SRC/build_$OS" >&2
  exit 1
fi
ls -l "$OUT/"
echo ">> Done."
