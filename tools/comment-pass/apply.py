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
#   - appears in an installed header (the headers cmake/ZengineInstall.cmake's code installs);
#   - appears inside a string or character literal in any C++ file, or at all in any file that
#     is neither C++ nor markdown (a pattern, a target, a data file);
#   - appears under a directory the pass may not touch: examples/ and third_party/ always, and
#     each directory a `# fence: <dir>/` line of the map names (a phase's own fence);
#   - is declared outside, or appears in C++ outside, the directories the map's `# scope: <dir>/`
#     lines name, when it has any (a phase that may rename only its own names; a scope directory
#     is not untouchable);
#   - is not in the file the row says declares it, or its new name already exists.
#
# In scope for the rename: every other tracked C++ and markdown file. Frozen history
# (docs/history/) and the quarry (reference/) keep the names they were written with, and this
# directory's map and demo name the rows by design; those are neither fenced nor renamed. A
# row's comment swap replaces exactly one occurrence of its before-text with its after-text,
# comment lines only.

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lex  # noqa: E402

MAP = "tools/comment-pass/renames.tsv"
TOOLS = "tools/comment-pass/"
FROZEN = ("docs/history/", "reference/")
UNTOUCHABLE = ("examples/", "third_party/")
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


def read_fences(path):
    """(fenced, scope): the directories the map's `# fence: <dir>/` lines add to the untouchable
    ones, less any its `# scope: <dir>/` lines name; and those scope directories."""
    fenced, scope = list(UNTOUCHABLE), []
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^# (fence|scope): (\S+/)\s*$", line.rstrip("\n"))
            if m:
                (fenced if m.group(1) == "fence" else scope).append(m.group(2))
    return tuple(d for d in fenced if d not in scope), tuple(scope)


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
    """The headers the install file's code installs; its comments name some it does not."""
    text = tree.text("cmake/ZengineInstall.cmake") or ""
    code = lex.without_comments(text, lex.cmake_spans(text))
    return sorted({p for p in re.findall(r"[A-Za-z0-9_./-]+\.(?:hpp|h)\b", code) if p in tree.paths})


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


def fences(row, tree, headers, fenced=UNTOUCHABLE, scope=()):
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
    if scope and not row["declared"].startswith(scope):
        raise Refused("%s is declared in %s, outside this phase's scope (%s)" % (
            old, row["declared"], " ".join(scope)))
    for path in tree.paths:
        base = path.rsplit("/", 1)[-1]
        if base == old or base.split(".")[0] == old:
            raise Refused("%s names a file: %s" % (old, path))
    for path in tree.grep(new):
        if not path.startswith(TOOLS):
            raise Refused("%s already exists (%s)" % (new, path))
    hits = []
    for path in tree.grep(old):
        text = tree.text(path)
        if text is not None and not path.startswith((TOOLS,) + FROZEN) and tok.search(text):
            hits.append((path, text))
    # what the name is comes first, everywhere it occurs; where it occurs, after
    for path, text in hits:
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
    for path, _ in hits:
        if path.startswith(fenced):
            raise Refused("%s appears in %s, under a directory this pass may not touch" % (old, path))
        if scope and lex.kind_of(path) == "cxx" and not path.startswith(scope):
            raise Refused("%s appears in %s, outside this phase's scope: that name stays" % (old, path))


def in_scope(path, fenced):
    return not path.startswith((TOOLS,) + FROZEN + fenced) and \
        (lex.kind_of(path) == "cxx" or path.endswith(".md"))


def apply_rows(rows, tree, fenced=UNTOUCHABLE, scope=()):
    """The renamed files, as {path: text}, and per-row {path: occurrences}."""
    headers = installed_headers(tree)
    for row in rows:
        fences(row, tree, headers, fenced, scope)
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
            if not in_scope(path, fenced):
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
    """Rows that must each be refused, one per fence, each for its own reason; and the
    installed headers read from the install file's code, C headers included."""
    demo = [
        ("authored_provider", "authored_office", "member", "workshop/arrangement_vocabulary.hpp",
         "a shape field", "inside a shape macro"),
        ("kPresenterRole", "kPresenterOffice", "constant", "workshop/pane_vocabulary.hpp",
         "an installed header", "in the installed header"),
        ("resolve_durable_path", "resolve_saved_path", "function", "workshop/user_paths.hpp",
         "a name inside a literal", "inside a literal"),
        ("pane_unit", "pane_units", "namespace", "workshop/setup.hpp", "a namespace",
         "is a namespace"),
        ("ZENGINE_BUILDER_CMAKE", "ZENGINE_BUILD_CMAKE", "constant", "workshop/workshop.cpp",
         "a name in a CMake file", "neither C++ nor markdown"),
        ("kMaxSetupNameLen", "kMaxSetupNameBytes", "constant", "workshop/setup.hpp",
         "a fenced directory (tests/)", "may not touch"),
        ("kMaxSetupNameLen", "kMaxSetupNameBytes", "constant", "workshop/setup.hpp",
         "a name declared outside the scope (tests/ examples/)", "outside this phase's scope ("),
        ("Condition", "PaneCondition", "type", "tests/workshop_support.hpp",
         "a test's name a package also uses", "that name stays"),
    ]
    headers = installed_headers(tree)
    ok = 0
    for old, new, kind, declared, what, reason in demo:
        row = {"old": old, "new": new, "kind": kind, "declared": declared}
        scoped = "scope" in what or "package also" in what
        try:
            if scoped:
                fences(row, tree, headers, ("third_party/",), ("tests/", "examples/"))
            else:
                fences(row, tree, headers, UNTOUCHABLE + ("tests/",))
            print("NOT REFUSED (%s): %s -> %s" % (what, old, new))
        except Refused as why:
            own = reason in str(why)
            ok += own
            print("refused (%s)%s: %s -> %s: %s" % (what, "" if own else " FOR ANOTHER REASON",
                                                    old, new, why))
    public = ("operator/host_abi.h", "operator/provider_abi.h", "flow/native_abi.h")
    private = ("surface/skin.hpp", "input/translate_sdl.hpp", "timer/timer_weave.hpp")
    have_c = all(h in headers for h in public)
    no_private = not any(h in headers for h in private)
    print("installed headers: %d, read from the install file's code; its C headers %s; the "
          "headers only its comments name %s" % (len(headers), "read" if have_c else "MISSING",
                                                 "left out" if no_private else "COUNTED"))
    print("fence demo: %d of %d rows refused for their own reason" % (ok, len(demo)))
    return 0 if ok == len(demo) and have_c and no_private else 1


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
    fenced, scope = read_fences(os.path.join(args.repo, args.map))
    try:
        files, counts = apply_rows(rows, tree, fenced, scope)
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
