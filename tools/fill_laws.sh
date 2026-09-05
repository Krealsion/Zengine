#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE LAWS-SUPPORTED GENERATOR -- rule k of AGENTS.md made mechanical.
#
# What it regenerates: the one generated line of every decision record under
# agents/decisions/, `**Laws supported.** [WL-…](../workshop/<register>.md), …`, from the
# registers' own WHY lines under every register directory the law-register check's family
# table names. A record's list is exactly the set of ids whose WHY names that record, in
# register order (families in table order, files alphabetical, entries in file order), wrapped
# at 98 bytes. The check (tests/check_law_register.cmake) asserts that a record lists exactly
# those laws; this is the tool that writes the list, so the line is never hand-edited.
#
# THE FAMILY TABLE HAS ONE COPY, and it is the check's. This tool reads the two spellings the
# check keeps at column 0, one line each -- `set(ZEN_LAW_FAMILIES WL ...)` and one
# `set(ZEN_LAW_DIR_<family> agents/<dir>)` per family -- and refuses to run when it finds no
# family, a family with no directory, or a directory that does not exist: over an unreadable
# table this tool would otherwise rewrite every record's list to nothing and call it current.
#
# A record carries either the generated paragraph or the placeholder `@@LAWS@@` on a line of its
# own (a new record); both are replaced. A record no WHY names is reported and left alone, and
# the check will then fail it, which is the right answer for a record nothing cites.
#
#   bash tools/fill_laws.sh              (from the repository root, or from anywhere)
#   bash tools/fill_laws.sh <repo>       (another checkout)
#
# The run is idempotent: over a tree whose records are current it changes no byte, which is
# how the tool is proven -- run it, and `git status` stays clean.
set -u
root="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$root" || exit 2
checker=tests/check_law_register.cmake
[ -d agents/decisions ] && [ -f "$checker" ] || { echo "not a repository root: $root" >&2; exit 2; }

families=$(sed -n 's/^set(ZEN_LAW_FAMILIES[[:space:]]\+\([A-Z][A-Z ]*\))[[:space:]]*$/\1/p' "$checker")
[ "$(printf '%s\n' "$families" | grep -c .)" -eq 1 ] || {
  echo "FAIL: $checker carries no single 'set(ZEN_LAW_FAMILIES ...)' row at column 0; found: '$families'" >&2
  exit 2
}
dirs=""
for fam in $families; do
  dir=$(sed -n "s/^set(ZEN_LAW_DIR_$fam[[:space:]]\+\([^) ]*\))[[:space:]]*$/\1/p" "$checker")
  [ "$(printf '%s\n' "$dir" | grep -c .)" -eq 1 ] || {
    echo "FAIL: family $fam has no single 'set(ZEN_LAW_DIR_$fam ...)' row in $checker; found: '$dir'" >&2
    exit 2
  }
  [ -d "$dir" ] || { echo "FAIL: family $fam names $dir, which is not a directory" >&2; exit 2; }
  dirs="$dirs $dir"
done
id_re=$(printf '%s' "$families" | tr ' ' '|')

# One line per WHY: `<record slug>\t<id>\t<dir basename>/<register file>`, in register order.
map=$(mktemp)
for dir in $dirs; do
  reldir=$(basename "$dir")
  for f in "$dir"/*.md; do
    base=$(basename "$f")
    awk -v F="$reldir/$base" -v RE="^## ($id_re)-" '
      $0 ~ RE { id = $2 }
      /^WHY (—|--) / {
        if (match($0, /`[^`]*`/)) {
          t = substr($0, RSTART + 1, RLENGTH - 2)
          sub(/^agents\/decisions\//, "", t)
          print t "\t" id "\t" F
        }
      }' "$f"
  done
done > "$map"
[ -s "$map" ] || { echo "FAIL: no WHY line names any record under$dirs" >&2; rm -f "$map"; exit 2; }

total=0
records=0
for r in agents/decisions/*.md; do
  slug=$(basename "$r")
  if ! grep -q '^@@LAWS@@$' "$r" && ! grep -q '^\*\*Laws supported\.\*\*' "$r"; then
    echo "skip (no placeholder and no Laws-supported paragraph): $slug"
    continue
  fi
  line=$(awk -F'\t' -v R="$slug" '$1==R { printf "[%s](../%s), ", $2, $3 }' "$map")
  if [ -z "$line" ]; then echo "FAIL: no WHY names $slug"; continue; fi
  line="**Laws supported.** ${line%, }."
  wrapped=$(printf '%s\n' "$line" | fold -s -w 98 | sed 's/ *$//')
  # Replace the placeholder, or the existing paragraph: from the `**Laws supported.**` line to
  # the line before the next blank line (or the end of the file).
  awk -v W="$wrapped" '
    /^@@LAWS@@$/ { print W; next }
    /^\*\*Laws supported\.\*\*/ { print W; skipping = 1; next }
    skipping && /^[ \t]*$/ { skipping = 0 }
    skipping { next }
    { print }' "$r" > "$r.tmp" && mv "$r.tmp" "$r"
  n=$(printf '%s\n' "$line" | grep -oE "($id_re)-[A-Z]+-[0-9]+" | wc -l)
  total=$((total + n))
  records=$((records + 1))
  printf '%-45s %3d laws\n' "$slug" "$n"
done
echo "families: $families; records written: $records; total laws listed: $total"
rm -f "$map"
