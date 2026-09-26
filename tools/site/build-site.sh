#!/bin/sh
# Build the web manual into docs/ (served by GitHub Pages from main:/docs) and one PDF per language
# into build/site/ (attached to the release). Adapted from proven_c_lib (same author, MIT).
#
# Sources: manual/ + spec/lowstruct.md (English), manual-ko/ + spec/lowstruct.ko.md (Korean). Nothing
# here edits them; everything in docs/en, docs/ko and docs/index.html is generated.
#
#   tools/site/build-site.sh
#
# Needs Typst 0.13+ with HTML export (TYPST=path to override) and the fonts Noto Sans/Serif, Noto Sans
# CJK KR and D2Coding (TYPST_FONT_PATHS). Typst does not fail on a missing font — it substitutes — so
# the PDF step checks its own warnings.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
work="$root/build/site"
out="$root/docs"

typst=${TYPST:-typst}
command -v "$typst" >/dev/null 2>&1 || typst="$root/../usr/toolchains/typst/typst"
[ -x "$typst" ] || { echo "build-site: no typst binary (set TYPST)" >&2; exit 1; }
fonts=${TYPST_FONT_PATHS:-$root/../usr/toolchains/fonts}

version=$(sed -n 's/^#define LOWSTRUCT_VERSION_STRING "\(.*\)"$/\1/p' "$root/c/include/lowstruct.h")
[ -n "$version" ] || { echo "build-site: cannot read the version" >&2; exit 1; }
pdf_pattern="https://github.com/rubidus-api/lowstruct/releases/download/v$version/lowstruct-$version-%s-manual.pdf"

sources() {   # sources <lang> -> "<md path> <page name>" lines
  if [ "$1" = en ]; then dir="$root/manual"; spec="$root/spec/lowstruct.md"; else dir="$root/manual-ko"; spec="$root/spec/lowstruct.ko.md"; fi
  for md in "$dir"/*.md; do
    n=$(basename "$md" .md); [ "$n" = README ] && n=index
    printf '%s %s\n' "$md" "$n"
  done
  printf '%s spec\n' "$spec"
}

build_lang() {
  lang=$1
  echo "build-site: $lang"
  rm -rf "$work/$lang" "$out/$lang"
  mkdir -p "$work/$lang/typ" "$work/$lang/frag" "$out/$lang"

  sources "$lang" | while read -r md name; do
    python3 "$here/md2typst.py" "$md" "$work/$lang/typ/$name.typ" "$lang" "$version"
  done

  for typ in "$work/$lang/typ"/*.typ; do
    name=$(basename "$typ" .typ)
    TYPST_FONT_PATHS="$fonts" "$typst" compile --root "$root" --features html --format html \
        "$typ" "$work/$lang/frag/$name.html" 2>"$work/$lang/frag/$name.log" \
        || { echo "build-site: $lang/$name failed to compile to HTML:" >&2; cat "$work/$lang/frag/$name.log" >&2; exit 1; }
  done

  pdf_url=$(printf "$pdf_pattern" "$lang")
  python3 "$here/site_pages.py" "$lang" "$work/$lang/frag" "$out/$lang" "$version" "$pdf_url"
  cp -f "$here/manual.css" "$out/$lang/manual.css"

  book="$work/$lang/manual.typ"
  {
    printf '#set document(title: "lowstruct %s", author: "lowstruct")\n' "$version"
    printf '#set page(paper: "a4", margin: (x: 2.2cm, y: 2.2cm), numbering: "1")\n'
    if [ "$lang" = ko ]; then
      printf '#set text(font: ("Noto Serif CJK KR", "Noto Serif"), size: 10pt, lang: "ko")\n'
    else
      printf '#set text(font: ("Noto Serif", "Noto Serif CJK KR"), size: 10pt, lang: "en")\n'
    fi
    printf '#show raw: set text(font: "D2Coding", size: 8.5pt)\n'
    printf '#set par(justify: false, leading: 0.72em)\n'
    printf '#show link: set text(fill: rgb("#1f5fa8"))\n'
    printf '#show heading: set block(above: 1.4em, below: 0.7em)\n'
    printf '#outline(depth: 2)\n#pagebreak()\n'
    for name in index guide python javascript c implementers spec; do
      [ -f "$work/$lang/typ/$name.typ" ] && printf '#include "typ/%s.typ"\n#pagebreak()\n' "$name"
    done
  } > "$book"
  TYPST_FONT_PATHS="$fonts" "$typst" compile --root "$root" "$book" "$work/lowstruct-$version-$lang-manual.pdf" \
      2>"$work/$lang/pdf.log" || { echo "build-site: $lang PDF failed:" >&2; cat "$work/$lang/pdf.log" >&2; exit 1; }
  if grep -q "unknown font family" "$work/$lang/pdf.log"; then
    echo "build-site: $lang PDF was set in substitute fonts:" >&2; grep "unknown font family" "$work/$lang/pdf.log" >&2; exit 1
  fi
  echo "build-site: $lang PDF -> build/site/lowstruct-$version-$lang-manual.pdf"
}

build_lang en
build_lang ko
: > "$out/.nojekyll"
python3 "$here/site_root.py" "$out" "$version" "$work/en/frag/contents.json" "$work/ko/frag/contents.json" "$pdf_pattern"
sh "$here/make-webfonts.sh"
python3 "$here/check-site-links.py" "$out"
echo "build-site: done -> docs/"
