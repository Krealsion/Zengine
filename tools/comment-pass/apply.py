# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The rename applier: the second of map -> applier -> proof (AGENTS.md rule o). It reads the
# map (renames.tsv), reads every file it may touch from a BASE commit rather than from the
# working tree, and writes the result -- so a rerun from the same base with the same map is the
# same tree, and striking a row is a rerun.
#
#   python tools/comment-pass/apply.py --base <commit>                 write the renamed tree
#   python tools/comment-pass/apply.py --base <commit> --check         judge the fences only
#   python tools/comment-pass/apply.py --base <commit> --fence-demo    show each fence refusing
#
# A row is refused, and nothing is written, when its old name
#   - is a namespace, macro, file, CMake target or artifact (its kind, or a `namespace` or
#     `#define` of it anywhere, or a file named for it);
#   - appears inside ZEN_SHAPE, ZEN_FIELD, ZEN_EXPOSE or ZEN_HIDE;
#   - appears in an installed header (cmake/ZengineInstall.cmake's lists);
#   - appears inside a string or character literal in any C++ file, or at all in any file that
#     is neither C++ nor markdown (a pattern, a target, a data file);
#   - is not in the file the row says declares it, or its new name already exists.
#
# In scope for the rename: every tracked C++ and markdown file except frozen history
# (docs/history/), the quarry (reference/), examples/ and this directory, whose map and demo
# name the rows by design and are neither fenced nor renamed. A row's comment swap replaces
# exactly one occurrence of its before-text with its after-text, comment lines only.

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lex  # noqa: E402

MAP = "tools/comment-pass/renames.tsv"
TOOLS = "tools/comment-pass/"
SKIP = ("docs/history/", "reference/", "examples/", "third_party/", TOOLS)
FENCED_KINDS = ("namespace", "macro", "file", "target", "artifact")
SHAPE_MACROS = re.compile(r"\bZEN_(?:SHAPE|FIELD|EXPOSE|HIDE)\s*\(")


class Refused(Exception):
    pass


def git(repo, *args):
    out = subprocess.run(["git", "-C", repo] + list(args), capture_output=True)
    if out.returncode != 0:
        raise SystemExit("git %s: %s" % (" ".join(args), out.stderr.decode("utf-8", "replace")))
    return out.stdout


def token(name):
    return re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])")


def read_map(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#") or line.startswith("old\t"):
                continue
            cells = line.split("\t")
            if len(cells) != 8:
                raise SystemExit("%s:%d: %d columns, want 8" % (path, number, len(cells)))
            old, new, kind, declared, why, cfile, before, after = cells
            rows.append({"old": old, "new": new, "kind": kind, "declared": declared, "why": why,
                         "comment_file": cfile, "before": before.replace("\\n", "\n"),
                         "after": after.replace("\\n", "\n"), "line": number})
    return rows


class Tree:
    """Every tracked file of a commit, read once."""

    def __init__(self, repo, commit):
        self.repo = repo
        self.commit = commit
        names = git(repo, "ls-tree", "-r", "--name-only", commit).decode("utf-8").split("\n")
        self.paths = [p for p in names if p]
        self._text = {}

    def grep(self, name):
        """The paths whose text holds `name` as a whole word, at this commit."""
        out = subprocess.run(["git", "-C", self.repo, "grep", "-l", "-w", "-F", "-e", name,
                              self.commit, "--"], capture_output=True)
        found = out.stdout.decode("utf-8").splitlines()
        prefix = self.commit + ":"
        return sorted(p[len(prefix):] for p in found if p.startswith(prefix))

    def text(self, path):
        if path not in self._text:
            raw = git(self.repo, "show", "%s:%s" % (self.commit, path))
            try:
                self._text[path] = raw.decode("utf-8")
            except UnicodeDecodeError:
                self._text[path] = None  # binary: no name lives in it
        return self._text[path]


def installed_headers(tree):
    text = tree.text("cmake/ZengineInstall.cmake") or ""
    return sorted({p for p in re.findall(r"[A-Za-z0-9_./-]+\.hpp", text) if p in tree.paths})


def shape_spans(text):
    """The argument text of every ZEN_SHAPE/ZEN_FIELD/ZEN_EXPOSE/ZEN_HIDE invocation."""
    out = []
    for m in SHAPE_MACROS.finditer(text):
        depth, i = 1, m.end()
        while i < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
        out.append(text[m.end():i])
    return out


def fences(row, tree, headers):
    """Raise Refused naming the first fence the row meets."""
    old, new = row["old"], row["new"]
    tok, newtok = token(old), token(new)
    if row["kind"] in FENCED_KINDS:
        raise Refused("%s is a %s: not renamed here" % (old, row["kind"]))
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", old or "") or \
            not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", new or ""):
        raise Refused("%s -> %s: both names must be identifiers" % (old, new))
    declared = tree.text(row["declared"]) if row["declared"] in tree.paths else None
    if declared is None or not tok.search(declared):
        raise Refused("%s does not appear in %s" % (old, row["declared"]))
    for path in tree.paths:
        base = path.rsplit("/", 1)[-1]
        if base == old or base.split(".")[0] == old:
            raise Refused("%s names a file: %s" % (old, path))
    for path in tree.grep(new):
        if not path.startswith(TOOLS):
            raise Refused("%s already exists (%s)" % (new, path))
    for path in tree.grep(old):
        text = tree.text(path)
        if text is None or path.startswith(TOOLS) or not tok.search(text):
            continue
        kind = lex.kind_of(path)
        if kind == "cxx":
            spans = lex.cxx_spans(text)
            for lit in lex.literals(text, spans):
                if tok.search(lit):
                    raise Refused("%s appears inside a literal in %s: %s" % (old, path, lit[:60]))
            code = lex.without_comments(text, spans)
            if re.search(r"\bnamespace\s+" + re.escape(old) + r"\b", code):
                raise Refused("%s is a namespace (%s)" % (old, path))
            if re.search(r"#\s*define\s+" + re.escape(old) + r"\b", code):
                raise Refused("%s is a macro (%s)" % (old, path))
            for args in shape_spans(code):
                if tok.search(args):
                    raise Refused("%s appears inside a shape macro in %s" % (old, path))
            if path in headers:
                raise Refused("%s appears in the installed header %s" % (old, path))
        elif not path.endswith(".md"):
            raise Refused("%s appears in %s, which is neither C++ nor markdown" % (old, path))


def in_scope(path):
    return not path.startswith(SKIP) and \
        (lex.kind_of(path) == "cxx" or path.endswith(".md"))


def apply_rows(rows, tree):
    """The renamed files, as {path: text}, and per-row {path: occurrences}."""
    headers = installed_headers(tree)
    for row in rows:
        fences(row, tree, headers)
    current = {}

    def get(path):
        return current[path] if path in current else tree.text(path)

    counts = []
    for row in rows:
        seen = {}
        if row["comment_file"]:
            text = get(row["comment_file"])
            if text.count(row["before"]) != 1:
                raise Refused("line %d: the comment before-text occurs %d times in %s" %
                              (row["line"], text.count(row["before"]), row["comment_file"]))
            for side in (row["before"], row["after"]):
                for line in side.split("\n"):
                    if not line.lstrip().startswith("//"):
                        raise Refused("line %d: a comment swap line is not a comment: %r" %
                                      (row["line"], line))
            current[row["comment_file"]] = text.replace(row["before"], row["after"], 1)
            seen[row["comment_file"] + " (comment)"] = 1
        tok = token(row["old"])
        for path in sorted(set(tree.grep(row["old"])) | set(current)):
            if not in_scope(path):
                continue
            text = get(path)
            if text is None:
                continue
            new_text, n = tok.subn(row["new"], text)
            if n:
                current[path] = new_text
                seen[path] = n
        counts.append(seen)
    return current, counts


def fence_demo(tree):
    """Rows that must each be refused, one per fence."""
    demo = [
        ("authored_provider", "authored_office", "member", "workshop/arrangement_vocabulary.hpp",
         "a shape field"),
        ("kPresenterRole", "kPresenterOffice", "constant", "workshop/pane_vocabulary.hpp",
         "an installed header"),
        ("resolve_durable_path", "resolve_saved_path", "function", "workshop/user_paths.hpp",
         "a name inside a literal"),
        ("pane_unit", "pane_units", "namespace", "workshop/setup.hpp", "a namespace"),
        ("ZENGINE_BUILDER_CMAKE", "ZENGINE_BUILD_CMAKE", "constant", "workshop/workshop.cpp",
         "a name in a CMake file"),
    ]
    headers = installed_headers(tree)
    refused = 0
    for old, new, kind, declared, what in demo:
        row = {"old": old, "new": new, "kind": kind, "declared": declared}
        try:
            fences(row, tree, headers)
            print("NOT REFUSED (%s): %s -> %s" % (what, old, new))
        except Refused as why:
            refused += 1
            print("refused (%s): %s -> %s: %s" % (what, old, new, why))
    print("fence demo: %d of %d rows refused" % (refused, len(demo)))
    return 0 if refused == len(demo) else 1


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--base", required=True)
    ap.add_argument("--map", default=MAP)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--fence-demo", action="store_true")
    args = ap.parse_args()
    tree = Tree(args.repo, args.base)
    if args.fence_demo:
        return fence_demo(tree)
    rows = read_map(os.path.join(args.repo, args.map))
    try:
        files, counts = apply_rows(rows, tree)
    except Refused as why:
        print("apply: refused -- %s; nothing written" % why)
        return 1
    for row, seen in zip(rows, counts):
        total = sum(n for p, n in seen.items() if not p.endswith("(comment)"))
        print("%s -> %s (%s, %s): %d occurrences" % (row["old"], row["new"], row["kind"],
                                                    row["declared"], total))
        for path in sorted(seen):
            print("    %-48s %d" % (path, seen[path]))
    if args.check:
        print("apply: %d rows pass every fence; nothing written (--check)" % len(rows))
        return 0
    for path, text in sorted(files.items()):
        with open(os.path.join(args.repo, path), "w", encoding="utf-8", newline="") as f:
            f.write(text)
    print("apply: %d rows, %d files written from %s" % (len(rows), len(files), args.base))
    return 0


if __name__ == "__main__":
    sys.exit(main())
