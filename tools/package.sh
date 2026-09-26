#!/bin/sh
# Package a release into build/dist/: the C library for Linux and Windows, install packs (zip) for
# Node.js and Python, and SHA256SUMS. Nothing is published to a package registry.
#
# Needs the native build (cd c && ../build/nob) and the Windows build (cd c && ../build/nob windows,
# on a machine with x86_64-w64-mingw32-gcc) to be done first.
# usage: tools/package.sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
ver=$(sed -n 's/^#define LOWSTRUCT_VERSION_STRING "\(.*\)"/\1/p' c/include/lowstruct.h)
dist=build/dist
rm -rf "$dist" && mkdir -p "$dist"

need() { [ -e "$1" ] || { echo "package: missing $1 — build it first" >&2; exit 1; }; }
need build/c/liblowstruct.a
need "build/c/liblowstruct.so.$ver"
need build/c/windows-x86_64/lowstruct.dll

# Common tree: headers (lowstruct.h and the four proven_c_lib headers it includes), licenses, a README.
common() {
  d=$1
  mkdir -p "$d/include/proven" "$d/lib"
  cp c/include/lowstruct.h "$d/include/"
  for h in types allocator error memory; do cp "c/vendor/proven/include/proven/$h.h" "$d/include/proven/"; done
  cp LICENSE "$d/LICENSE"
  cp c/vendor/proven/LICENSE "$d/LICENSE-proven_c_lib"
  cat > "$d/README.md" <<EOT
# lowstruct $ver — C library ($2)

Parser for lowstruct (\`.lows\`), a small, strict configuration file format.
Manual: https://github.com/rubidus-api/lowstruct/blob/v$ver/manual/c.md

Compile with \`-std=c23\` (C11 and C17 also compile the header) and \`-Iinclude\`.

$3

API: \`include/lowstruct.h\`. The headers under \`include/proven/\` come from proven_c_lib (MIT, see
\`LICENSE-proven_c_lib\`); lowstruct's own license is \`LICENSE\`.
EOT
}

# Linux x86_64
L="lowstruct-$ver-linux-x86_64"
common "$dist/$L" "Linux x86_64" "- Static: link \`lib/liblowstruct.a -lm\`.
- Shared: define \`LOWS_SHARED\`, link \`-Llib -llowstruct\`; ship \`liblowstruct.so.0\` with your program.
  The shared library exports only the \`lows_*\` API and needs glibc 2.14 or later."
cp build/c/liblowstruct.a "$dist/$L/lib/"
cp "build/c/liblowstruct.so.$ver" "$dist/$L/lib/"
ln -s "liblowstruct.so.$ver" "$dist/$L/lib/liblowstruct.so.0"
ln -s liblowstruct.so.0 "$dist/$L/lib/liblowstruct.so"
(cd "$dist" && tar -czf "$L.tar.gz" "$L")

# Windows x86_64 (MinGW-w64, UCRT)
W="lowstruct-$ver-windows-x86_64"
common "$dist/$W" "Windows x86_64" "- Static: link \`lib/liblowstruct.a\` (MinGW-w64).
- DLL: define \`LOWS_SHARED\`, link \`lib/liblowstruct.dll.a\` (MinGW-w64), and ship \`bin/lowstruct.dll\`.
  The DLL depends only on the Windows Universal C Runtime (Windows 10 and later).
- \`lib/lowstruct.def\` lists the exports; with MSVC tools, \`lib /def:lib\\\\lowstruct.def /machine:x64 /out:lowstruct.lib\`
  makes an import library (not tested with MSVC)."
mkdir -p "$dist/$W/bin"
cp build/c/windows-x86_64/liblowstruct.a build/c/windows-x86_64/liblowstruct.dll.a build/c/windows-x86_64/lowstruct.def "$dist/$W/lib/"
cp build/c/windows-x86_64/lowstruct.dll "$dist/$W/bin/"
(cd "$dist" && zip -qr "$W.zip" "$W")

# Node.js and Python install packs: the package directory as it is in the repository, installable from
# the unpacked folder without a registry.
N="lowstruct-$ver-node"
mkdir -p "$dist/$N"
cp -r js/src js/package.json js/README.md js/LICENSE "$dist/$N/"
cat > "$dist/$N/INSTALL.md" <<EOT
# Installing lowstruct $ver for Node.js

Node.js 20 or later. From the folder that contains this one:

    npm install ./$N

Then \`import { parse } from "lowstruct";\`. Usage: README.md in this folder, and
https://github.com/rubidus-api/lowstruct/blob/v$ver/manual/javascript.md
EOT
P="lowstruct-$ver-python"
mkdir -p "$dist/$P/lowstruct"
cp python/lowstruct/__init__.py "$dist/$P/lowstruct/"
cp python/pyproject.toml python/README.md python/LICENSE "$dist/$P/"
cat > "$dist/$P/INSTALL.md" <<EOT
# Installing lowstruct $ver for Python

Python 3.10 or later. From the folder that contains this one:

    pip install ./$P

or, without installing, put this folder on \`PYTHONPATH\`. Then \`import lowstruct\`. Usage: README.md in
this folder, and https://github.com/rubidus-api/lowstruct/blob/v$ver/manual/python.md
EOT
(cd "$dist" && zip -qr "$N.zip" "$N" && zip -qr "$P.zip" "$P")

(cd "$dist" && rm -rf "$L" "$W" "$N" "$P" && sha256sum -- * > SHA256SUMS && cat SHA256SUMS)
