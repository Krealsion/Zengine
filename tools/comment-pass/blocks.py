# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Print a file's comments, not the file: each block as its reader meets it -- comment lines with
# only blank lines between -- with its line numbers, what source_comments would say of it, and
# the code line it sits on; then each trailing comment with its line. Read-only.
#
#   python tools/comment-pass/blocks.py FILE... [--flagged] [--brief]
#
# --flagged prints only the blocks the check would flag; --brief prints each block's first line
# only. The id families and the block limit are read from the check itself; its verdict rules.

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lex  # noqa: E402

CHECK = "tests/check_source_comments.cmake"
ID_TOKEN = re.compile(r"(?<![A-Za-z0-9_-])[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-[0-9]+[a-z]?(?![A-Za-z0-9_])")
UNCOUNTED = re.compile(r"^\s*(?://|#) *(?:[A-Z][A-Za-z]* law:|SPDX-License-Identifier:|Copyright \(c\))"
                       r"|^\s*// [A-Z]+-[A-Z]+-[0-9]")
REMOVAL = re.compile(r"(?:was|were) here|used to be here|what used to be", re.I)
ONE_DIGIT = re.compile(r"-[0-9][a-z]?$")  # a law family's letters, a one-digit number: a phase tag


def check_setting(repo, name):
    """The words of one `set(NAME ...)` in the check's code."""
    with open(os.path.join(repo, CHECK), encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"set\(" + name + r"\s+([^)]*)\)", lex.without_comments(text, lex.cmake_spans(text)))
    return m.group(1).split() if m else []


def installed_headers(repo):
    with open(os.path.join(repo, "cmake/ZengineInstall.cmake"), encoding="utf-8") as f:
        text = f.read()
    code = lex.without_comments(text, lex.cmake_spans(text))
    return set(re.findall(r"[A-Za-z0-9_./-]+\.(?:hpp|h)\b", code))


def verdicts(text, families, not_ids):
    said = []
    if "⭐" in text or REMOVAL.search(text):
        said.append("REMOVAL NOTE")
    ids = [t for t in ID_TOKEN.findall(text)
           if t not in not_ids and (t.split("-", 1)[0] not in families or ONE_DIGIT.search(t))]
    if ids:
        said.append("ids " + " ".join(sorted(set(ids))))
    return said


def show(repo, rel, args, families, not_ids, limit, installed):
    with open(os.path.join(repo, rel), encoding="utf-8", newline="") as f:
        text = f.read().replace("\r\n", "\n")
    spans = lex.spans_of(rel, text)
    classes = [c for c, _, _ in lex.line_bytes(text, spans)]
    lines = text.split("\n")
    blocks, k = [], 0
    while k < len(classes):
        if classes[k] != "comment":
            k += 1
            continue
        start = end = k
        while end + 1 < len(classes) and classes[end + 1] in ("comment", "blank"):
            end += 1
        while classes[end] == "blank":
            end -= 1
        blocks.append((start, end))
        k = end + 1
    out = []
    for start, end in blocks:
        run = longest = 0
        for j in range(start, end + 1):
            if classes[j] == "comment" and not UNCOUNTED.match(lines[j]):
                run += 1
                longest = max(longest, run)
            elif classes[j] != "comment":
                run = 0
        said = verdicts("\n".join(lines[start:end + 1]), families, not_ids)
        if longest > limit and rel not in installed:
            said.insert(0, "LONG %d" % longest)
        if args.flagged and not said:
            continue
        count = sum(1 for j in range(start, end + 1) if classes[j] == "comment")
        below = next((j for j in range(end + 1, len(classes)) if classes[j] == "code"), None)
        out.append("-- %d-%d: %d comment lines%s" % (start + 1, end + 1, count,
                                                    ("  [" + "; ".join(said) + "]") if said else ""))
        for j in range(start, (start + 1) if args.brief else (end + 1)):
            out.append("%6d| %s" % (j + 1, lines[j]))
        out.append("   => %s" % ("%d| %s" % (below + 1, lines[below].strip()[:90]) if below is not None
                                 else "(end of file)"))
    for kind, s, e in spans:
        if kind != lex.COMMENT:
            continue
        j = text.count("\n", 0, s)
        if classes[j] == "code":
            said = verdicts(text[s:e], families, not_ids)
            if args.flagged and not said:
                continue
            out.append("-- %d trailing%s: %s" % (j + 1, ("  [" + "; ".join(said) + "]") if said else "",
                                                lines[j].strip()[:110]))
    total = sum(1 for c in classes if c == "comment")
    print("== %s: %d blocks, %d comment lines%s" % (rel, len(blocks), total,
                                                    " (installed header)" if rel in installed else ""))
    for line in out:
        print(line)


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--flagged", action="store_true")
    ap.add_argument("--brief", action="store_true")
    args = ap.parse_args()
    families = set(check_setting(args.repo, "ZEN_COMMENT_ID_FAMILIES"))
    not_ids = set(check_setting(args.repo, "ZEN_COMMENT_NOT_IDS"))
    limit = int((check_setting(args.repo, "ZEN_COMMENT_BLOCK_LIMIT") or ["6"])[0])
    installed = installed_headers(args.repo)
    for rel in args.files:
        show(args.repo, rel.replace("\\", "/"), args, families, not_ids, limit, installed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
