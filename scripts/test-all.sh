#!/bin/sh
# Run the conformance suite in all three implementations (C, Node.js, Python).
# usage: scripts/test-all.sh            quick
#        scripts/test-all.sh --fuzz     also the three-way differential fuzz (needs cc and node)
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
echo "== python"; python3 -m unittest discover -s python/tests
echo "== node";   (cd js && node --test)
echo "== c"
mkdir -p build
[ -x build/nob ] || (cd c && cc -o ../build/nob nob.c)
(cd c && ../build/nob)
if [ "${1:-}" = "--fuzz" ]; then
  echo "== difffuzz"; python3 tests/fuzz/difffuzz.py 4000 "$(date +%s)"
fi
echo "test-all: ok"
