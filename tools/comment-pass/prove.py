# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The comment pass's proof: the third of map -> applier -> proof (AGENTS.md rule o). It
# regenerates nothing. For every C++ and CMake file in the repository -- the start commit's
# and the working tree's -- it strips comments, collapses whitespace, drops blank lines, and
# asks whether START with the rename map applied to its code equals END, line for line, and
# whether the two carry the same literals. The one allowed difference is the new check: its
# own file, and its one registration line in tests/CMakeLists.txt.
#
#   python tools/comment-pass/prove.py --start <commit>           the proof
#   python tools/comment-pass/prove.py --start <commit> --demo    ...and show what it catches

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apply  # noqa: E402
import lex  # noqa: E402

CHECK_FILE = "tests/check_source_comments.cmake"
REGISTRATION_FILE = "tests/CMakeLists.txt"
REGISTRATION = "zengine_script_test(source_comments ${CMAKE_CURRENT_SOURCE_DIR}/check_source_comments.cmake)"


def git_lines(repo, *args):
    out = subprocess.run(["git", "-C", repo] + list(args), capture_output=True, check=True)
    return [p for p in out.stdout.decode("utf-8").splitlines() if p]


def read_start(repo, commit, paths):
    """{path: text} for every path at the commit, through one `git cat-file --batch`."""
    names = "".join("%s:%s\n" % (commit, p) for p in paths).encode("utf-8")
    out = subprocess.run(["git", "-C", repo, "cat-file", "--batch"], input=names,
                         capture_output=True, check=True).stdout
    texts, i = {}, 0
    for p in paths:
        header_end = out.index(b"\n", i)
        size = int(out[i:header_end].split()[2])
        body = out[header_end + 1:header_end + 1 + size]
        texts[p] = body.decode("utf-8", "replace").replace("\r\n", "\n")
        i = header_end + 1 + size + 1
    return texts


def code_form(path, text, rows):
    """(normalized code lines, literals), with the map's renames applied to code only."""
    spans = lex.spans_of(path, text)
    if rows and lex.kind_of(path) == "cxx":
        parts = []
        for kind, s, e in spans:
            piece = text[s:e]
            if kind == lex.CODE:
                for row in rows:
                    piece = apply.token(row["old"]).sub(row["new"], piece)
            parts.append((kind, piece))
        text = "".join(p for _, p in parts)
        spans = lex.spans_of(path, text)
    return lex.normalized_lines(text, spans), lex.literals(text, spans)


def compare(path, start_text, end_text, rows):
    """None when equal, else the first difference in words."""
    a_lines, a_lits = code_form(path, start_text, rows)
    b_lines, b_lits = code_form(path, end_text, [])
    if path == REGISTRATION_FILE:
        if b_lines.count(REGISTRATION) != 1:
            return "the check's registration line is not there exactly once"
        b_lines.remove(REGISTRATION)
    if a_lits != b_lits:
        for k, (x, y) in enumerate(zip(a_lits, b_lits)):
            if x != y:
                return "literal %d: %s -> %s" % (k, x[:60], y[:60])
        return "literal count %d -> %d" % (len(a_lits), len(b_lits))
    if a_lines != b_lines:
        for k, (x, y) in enumerate(zip(a_lines, b_lines)):
            if x != y:
                return "code line %d: %s  ->  %s" % (k + 1, x[:70], y[:70])
        return "code line count %d -> %d" % (len(a_lines), len(b_lines))
    return None


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--start", required=True)
    ap.add_argument("--map", default=apply.MAP)
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()
    repo = args.repo
    rows = apply.read_map(os.path.join(repo, args.map))
    start = {p for p in git_lines(repo, "ls-tree", "-r", "--name-only", args.start)
             if lex.kind_of(p)}
    end = {p for p in git_lines(repo, "ls-files") + git_lines(
        repo, "ls-files", "--others", "--exclude-standard") if lex.kind_of(p)}
    end = {p for p in end if os.path.exists(os.path.join(repo, p))}
    start_text = read_start(repo, args.start, sorted(start))
    failures = []
    for p in sorted(start - end):
        failures.append("%s: removed" % p)
    for p in sorted(end - start):
        if p != CHECK_FILE:
            failures.append("%s: added" % p)
    if CHECK_FILE not in end:
        failures.append("%s: the check is missing" % CHECK_FILE)
    compared = 0
    for p in sorted(start & end):
        with open(os.path.join(repo, p), encoding="utf-8", newline="") as f:
            end_text = f.read().replace("\r\n", "\n")
        compared += 1
        why = compare(p, start_text[p], end_text, rows)
        if why:
            failures.append("%s: %s" % (p, why))
    print("prove: %d C++ and CMake files compared, START with %d renames against the working "
          "tree; the check's file and its registration line set aside" % (compared, len(rows)))
    for f in failures:
        print("  DIFFERS  " + f)
    print("prove: %s" % ("identical" if not failures else "%d differences" % len(failures)))
    status = 0 if not failures else 1
    if args.demo:
        status |= demo(repo, start_text, rows)
    return status


def demo(repo, start_text, rows):
    """Three edits to one END file, in memory: a code token and a literal must be caught, and
    a comment-only edit must not be."""
    path = "workshop/setup.hpp"
    with open(os.path.join(repo, path), encoding="utf-8") as f:
        end_text = f.read()
    code = end_text.replace("s.panes.size() > kMaxSetupPanes", "s.panes.size() > kMaxSetupPanez", 1)
    literal = end_text.replace('"a setup name cannot be empty"', '"a setup name can not be empty"', 1)
    comment = end_text.replace("// ---- The bounds", "// ---- The limits", 1)
    ok = 0
    for what, text, want in (("one-token change", code, True), ("literal change", literal, True),
                             ("comment-only change", comment, False)):
        if text == end_text:
            print("demo: %s: the edit found nothing to change" % what)
            continue
        why = compare(path, start_text[path], text, rows)
        caught = why is not None
        print("demo: %s in %s: %s%s" % (what, path, "caught" if caught else "not a difference",
                                       (" -- " + why) if why else ""))
        ok += caught == want
    print("demo: %d of 3 as expected" % ok)
    return 0 if ok == 3 else 1


if __name__ == "__main__":
    sys.exit(main())
