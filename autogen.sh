#!/bin/sh
# Copyright (c) 2013-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

if [[ "$OSTYPE" =~ ^darwin ]]; then

rm -f configure >/dev/null || true
rm -f configure~ >/dev/null || true

echo "example:"
echo "/bin/sh ./configure --disable-option-checking '--prefix=/usr/local'  '--with-gui=qt@5' '--disable-tests' '--disable-shared' '--with-pic' '--enable-benchmark=no' '--enable-module-recovery' '--disable-module-ecdh' --cache-file=/dev/null --srcdir=."

type -P convert && echo "convert needed for gui build"|| brew install imagemagick ## needed for gui
type -P rsvg-convert && echo "rsvg-convert needed for gui build"|| brew install librsvg ## needed for gui
fi

export LC_ALL=C
set -e
srcdir="$(dirname "$0")"
cd "$srcdir"
if [ -z "${LIBTOOLIZE}" ] && GLIBTOOLIZE="$(command -v glibtoolize)"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
command -v autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all

if expr "'$(build-aux/config.guess --timestamp)" \< "'$(depends/config.guess --timestamp)" > /dev/null; then
  chmod ug+w build-aux/config.guess
  chmod ug+w src/secp256k1/build-aux/config.guess
  cp depends/config.guess build-aux
  cp depends/config.guess src/secp256k1/build-aux
fi
if expr "'$(build-aux/config.sub --timestamp)" \< "'$(depends/config.sub --timestamp)" > /dev/null; then
  chmod ug+w build-aux/config.sub
  chmod ug+w src/secp256k1/build-aux/config.sub
  cp depends/config.sub build-aux
  cp depends/config.sub src/secp256k1/build-aux
fi
