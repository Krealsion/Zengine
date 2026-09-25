# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Measure the comments of a tree or a commit: per file, comment lines and bytes, code lines
# and bytes; and the id-shaped tokens its comments cite. Read-only.
#
#   python tools/comment-pass/census.py [--commit <rev>] [--prefix workshop/] [--ids]

import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lex  # noqa: E402

ID_TOKEN = re.compile(r"(?<![A-Za-z0-9_-])[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-[0-9]+[a-z]?(?![A-Za-z0-9_])")


def files_at(repo, commit, prefix):
    if commit:
        out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "--name-only", commit, prefix],
                             capture_output=True, text=True, check=True).stdout
    else:
        out = subprocess.run(["git", "-C", repo, "ls-files", prefix],
                             capture_output=True, text=True, check=True).stdout
    return [p for p in out.splitlines() if lex.kind_of(p)]


def read_at(repo, commit, path):
    if commit:
        raw = subprocess.run(["git", "-C", repo, "show", "%s:%s" % (commit, path)],
                             capture_output=True, check=True).stdout
    else:
        with open(os.path.join(repo, path), "rb") as f:
            raw = f.read()
    return raw.decode("utf-8").replace("\r\n", "\n")


def measure(text, path):
    spans = lex.spans_of(path, text)
    rows = lex.line_bytes(text, spans)
    m = collections.Counter()
    m["files"] = 1
    m["bytes"] = len(text.encode("utf-8"))
    for cls, com, code in rows:
        m[cls + "_lines"] += 1
        m["comment_bytes"] += com
        m["code_bytes"] += code
    ids = collections.Counter()
    for s, e, body in lex.comments(text, spans):
        for tok in ID_TOKEN.findall(body):
            ids[tok] += 1
        m["star_notes"] += body.count("⭐")
    return m, ids


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--commit", default=None)
    ap.add_argument("--prefix", default="workshop/")
    ap.add_argument("--ids", action="store_true", help="list the id-shaped tokens comments cite")
    ap.add_argument("--top", type=int, default=5)
    args = ap.parse_args()
    total = collections.Counter()
    per_file = {}
    ids = collections.Counter()
    for path in files_at(args.repo, args.commit, args.prefix):
        m, i = measure(read_at(args.repo, args.commit, path), path)
        per_file[path] = m
        total.update(m)
        ids.update(i)
    nonblank = total["comment_lines"] + total["code_lines"]
    print("population: %d files under %s at %s" % (total["files"], args.prefix, args.commit or "the working tree"))
    print("bytes %d; comment bytes %d (%.1f%%); code bytes %d" % (
        total["bytes"], total["comment_bytes"], 100.0 * total["comment_bytes"] / max(1, total["bytes"]), total["code_bytes"]))
    print("lines: comment %d, code %d, blank %d; comment share of non-blank %.1f%%" % (
        total["comment_lines"], total["code_lines"], total["blank_lines"], 100.0 * total["comment_lines"] / max(1, nonblank)))
    print("star notes (U+2B50) in comments: %d" % total["star_notes"])
    print("largest %d files by bytes:" % args.top)
    for path, m in sorted(per_file.items(), key=lambda kv: -kv[1]["bytes"])[:args.top]:
        print("  %-40s bytes %8d  comment lines %5d  comment bytes %7d  code lines %5d" % (
            path, m["bytes"], m["comment_lines"], m["comment_bytes"], m["code_lines"]))
    if args.ids:
        fam = collections.Counter()
        for tok, n in ids.items():
            fam[re.sub(r"-[0-9]+[a-z]?$", "", tok)] += n
        print("id-shaped tokens in comments, by family:")
        for f, n in fam.most_common():
            print("  %-16s %d" % (f, n))


if __name__ == "__main__":
    main()
