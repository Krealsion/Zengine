#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE LAWS-SUPPORTED GENERATOR -- rule k of AGENTS.md made mechanical.
#
# What it regenerates: the one generated line of every decision record under
# agents/decisions/, `**Laws supported.** [WL-…](../workshop/<register>.md), …`, from the
# registers' own WHY lines under agents/workshop/. A record's list is exactly the set of ids
# whose WHY names that record, in register order (files alphabetical, entries in file order),
# wrapped at 98 bytes. The check (tests/check_law_register.cmake) asserts that a record lists
# exactly those laws; this is the tool that writes the list, so the line is never hand-edited.
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
[ -d agents/decisions ] && [ -d agents/workshop ] || { echo "not a repository root: $root" >&2; exit 2; }

map=$(mktemp)
for f in agents/workshop/*.md; do
  base=$(basename "$f")
  awk -v F="$base" '
    /^## WL-/ { id = $2 }
    /^WHY (—|--) / {
      if (match($0, /`[^`]*`/)) {
        t = substr($0, RSTART + 1, RLENGTH - 2)
        sub(/^agents\/decisions\//, "", t)
        print t "\t" id "\t" F
      }
    }' "$f"
done > "$map"

total=0
records=0
for r in agents/decisions/*.md; do
  slug=$(basename "$r")
  if ! grep -q '^@@LAWS@@$' "$r" && ! grep -q '^\*\*Laws supported\.\*\*' "$r"; then
    echo "skip (no placeholder and no Laws-supported paragraph): $slug"
    continue
  fi
  line=$(awk -F'\t' -v R="$slug" '$1==R { printf "[%s](../workshop/%s), ", $2, $3 }' "$map")
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
  n=$(printf '%s\n' "$line" | grep -o 'WL-[A-Z]*-[0-9]*' | wc -l)
  total=$((total + n))
  records=$((records + 1))
  printf '%-45s %3d laws\n' "$slug" "$n"
done
echo "records written: $records; total laws listed: $total"
rm -f "$map"
