#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
#
# THE LAWS-SUPPORTED GENERATOR -- rule k of CONTRIBUTING.md's register rules made mechanical.
#
# What it regenerates: the one generated line of every decision record,
# `**Laws supported.** [WL-…](../workshop/<register>.md), …`, from the registers' own WHY lines
# under every register directory the law-register check's family table names, and from the law
# pages of the folders tests/front_page_folders.txt names: there a law of a table family
# (`## WL-…-NN — `) has for its WHY the one decision record its section links (a page holding
# `**Decision record.**`). A record's list is exactly the set of ids whose WHY names that record,
# each linked to the file that declares it from the record's own folder, in register order
# (families in table order, files in the shell's glob order, entries in file order) and then
# law-page order (folders in the list's order, pages in glob order), wrapped at 98 bytes. The
# records written are those under agents/decisions/ and any other a law page's section links. The
# check (tests/check_law_register.cmake) asserts that a record lists exactly those laws, each
# linked to the file that declares it; this is the tool that writes the list, so the line is never
# hand-edited.
#
# THE FAMILY TABLE HAS ONE COPY, and it is the check's. This tool reads the two spellings the
# check keeps at column 0, one line each -- `set(ZEN_LAW_FAMILIES WL ...)` and one
# `set(ZEN_LAW_DIR_<family> agents/<dir>)` per family -- and refuses to run when it finds no
# family, a family with no directory, or a directory that does not exist: over an unreadable
# table this tool would otherwise rewrite every record's list to nothing and call it current.
#
# A record carries either the generated paragraph or the placeholder `@@LAWS@@` on a line of its
# own (a new record); both are replaced. A record no WHY names is reported and left alone, and
# the check will then fail it, which is the right answer for a record nothing cites. A law page's
# law whose section links two records is reported, and the tool exits 1.
#
#   bash tools/fill_laws.sh              (from the repository root, or from anywhere)
#   bash tools/fill_laws.sh <repo>       (another checkout)
#
# The run is idempotent: over a tree whose records are current it changes no byte, which is
# how the tool is proven -- run it, and `git status` stays clean. The order within a list follows
# the shell's collation of file names, which the check does not read.
set -u
root="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$root" || exit 2
checker=tests/check_law_register.cmake
front=tests/front_page_folders.txt
[ -d agents/decisions ] && [ -f "$checker" ] && [ -f "$front" ] || { echo "not a repository root: $root" >&2; exit 2; }

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

# One line per WHY, `<record path>\t<id>\t<declaring file path>`, every path from the repository
# root: the registers' in register order, then the law pages' in law-page order.
map=$(mktemp)
for dir in $dirs; do
  for f in "$dir"/*.md; do
    awk -v F="$f" -v RE="^## ($id_re)-" '
      $0 ~ RE { id = $2 }
      /^WHY (—|--) / {
        if (match($0, /`[^`]*`/)) {
          print substr($0, RSTART + 1, RLENGTH - 2) "\t" id "\t" F
        }
      }' "$f"
  done
done > "$map"
[ -s "$map" ] || { echo "FAIL: no WHY line names any record under$dirs" >&2; rm -f "$map"; exit 2; }

# A law page's law of a table family: every link in its section, from its `## <id> — ` heading to
# the next `## `, resolved from the page's folder; the one that is a decision record is its WHY.
pagefail=0
while IFS= read -r folder; do
  folder=${folder%$'\r'}
  case "$folder" in ''|'#'*) continue ;; esac
  for page in "$folder"/docs/*-laws.md; do
    [ -f "$page" ] || continue
    pagedir=$(dirname "$page")
    links=$(awk -v RE="^## ($id_re)-[A-Z]+-[0-9]+ " '
      /^## / { id = ($0 ~ RE) ? $2 : "" }
      id != "" {
        line = $0
        while (match(line, /\]\([^)#]+/)) {
          print id "\t" substr(line, RSTART + 2, RLENGTH - 2)
          line = substr(line, RSTART + RLENGTH)
        }
      }' "$page")
    [ -n "$links" ] || continue
    records=$(printf '%s\n' "$links" | while IFS=$'\t' read -r id target; do
      path=$(realpath -m --relative-to=. "$pagedir/$target")
      case "$path" in *.md) ;; *) continue ;; esac
      if [ -f "$path" ] && grep -qF '**Decision record.**' "$path"; then
        printf '%s\t%s\n' "$id" "$path"
      fi
    done | awk '!seen[$0]++')
    [ -n "$records" ] || continue
    for id in $(printf '%s\n' "$records" | cut -f1 | awk '!seen[$0]++'); do
      linked=$(printf '%s\n' "$records" | awk -F'\t' -v I="$id" '$1 == I { print $2 }')
      if [ "$(printf '%s\n' "$linked" | grep -c .)" -gt 1 ]; then
        echo "FAIL: $page $id links more than one decision record:" $linked
        pagefail=1
        continue
      fi
      printf '%s\t%s\t%s\n' "$linked" "$id" "$page" >> "$map"
    done
  done
done < "$front"

total=0
records=0
for r in $( (ls agents/decisions/*.md; cut -f1 "$map") | awk '!seen[$0]++'); do
  slug=$(basename "$r")
  [ -f "$r" ] || { echo "FAIL: a WHY names $r, which does not exist"; continue; }
  if ! grep -q '^@@LAWS@@$' "$r" && ! grep -q '^\*\*Laws supported\.\*\*' "$r"; then
    echo "skip (no placeholder and no Laws-supported paragraph): $slug"
    continue
  fi
  recdir=$(dirname "$r")
  line=$(awk -F'\t' -v R="$r" '$1 == R { print $2 "\t" $3 }' "$map" | while IFS=$'\t' read -r id file; do
    printf '[%s](%s), ' "$id" "$(realpath -m --relative-to="$recdir" "$file")"
  done)
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
[ "$pagefail" -eq 0 ] || exit 1
