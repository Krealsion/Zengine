# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Measure the comments of a tree or a commit: per file, comment lines and bytes, code lines
# and bytes; and the id-shaped tokens its comments cite. Read-only.
#
#   python tools/comment-pass/census.py [--commit <rev>] [--prefix workshop/] [--ids] [--by-dir]
#   python tools/comment-pass/census.py --path CMakeLists.txt --path ':(glob)tests/*.cmake' ...
#
# --prefix "" measures the whole repository; --path measures git pathspecs instead, one per flag
# (`:(glob)` makes `*` stop at a slash). --by-dir adds the totals of each directory directly
# under the prefix, heaviest comments first.

import argparse
import collections
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lex  # noqa: E402

ID_TOKEN = re.compile(r"(?<![A-Za-z0-9_-])[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-[0-9]+[a-z]?(?![A-Za-z0-9_])")


def selected(path, specs):
    """Whether a path matches git-style pathspecs: a path or a directory prefix, `:(glob)` for
    a glob whose `*` stops at a slash, `:(exclude)` or `:!` to take matches back out."""
    def one(spec):
        if spec.startswith(":(glob)"):
            return re.fullmatch(glob.translate(spec[7:], recursive=True, include_hidden=True), path)
        spec = spec.rstrip("/")
        return path == spec or path.startswith(spec + "/")
    keep, drop = [], []
    for s in specs:
        if s.startswith(":(exclude)"):
            drop.append(s[len(":(exclude)"):])
        elif s.startswith(":!"):
            drop.append(s[2:])
        else:
            keep.append(s)
    return any(one(s) for s in keep) and not any(one(s) for s in drop)


def files_at(repo, commit, prefix, paths=None):
    where = [prefix] if prefix and not paths else []
    if commit:
        out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "--name-only", commit] + where,
                             capture_output=True, text=True, check=True).stdout
    else:
        out = subprocess.run(["git", "-C", repo, "ls-files"] + where,
                             capture_output=True, text=True, check=True).stdout
    return [p for p in out.splitlines() if lex.kind_of(p) and (not paths or selected(p, paths))]


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
    ap.add_argument("--path", action="append", metavar="PATHSPEC",
                    help="a git pathspec to measure in place of --prefix; repeatable")
    ap.add_argument("--ids", action="store_true", help="list the id-shaped tokens comments cite")
    ap.add_argument("--top", type=int, default=5)
    ap.add_argument("--by-dir", action="store_true",
                    help="the totals of each directory directly under the prefix")
    args = ap.parse_args()
    if args.path:
        args.prefix = ""
    total = collections.Counter()
    per_file = {}
    ids = collections.Counter()
    for path in files_at(args.repo, args.commit, args.prefix, args.path):
        m, i = measure(read_at(args.repo, args.commit, path), path)
        per_file[path] = m
        total.update(m)
        ids.update(i)
    nonblank = total["comment_lines"] + total["code_lines"]
    where = " ".join(args.path) if args.path else (args.prefix or "the repository root")
    print("population: %d files under %s at %s" % (total["files"], where,
                                                  args.commit or "the working tree"))
    print("bytes %d; comment bytes %d (%.1f%%); code bytes %d" % (
        total["bytes"], total["comment_bytes"], 100.0 * total["comment_bytes"] / max(1, total["bytes"]), total["code_bytes"]))
    print("lines: comment %d, code %d, blank %d; comment share of non-blank %.1f%%" % (
        total["comment_lines"], total["code_lines"], total["blank_lines"], 100.0 * total["comment_lines"] / max(1, nonblank)))
    print("star notes (U+2B50) in comments: %d" % total["star_notes"])
    print("largest %d files by bytes:" % args.top)
    for path, m in sorted(per_file.items(), key=lambda kv: -kv[1]["bytes"])[:args.top]:
        print("  %-40s bytes %8d  comment lines %5d  comment bytes %7d  code lines %5d" % (
            path, m["bytes"], m["comment_lines"], m["comment_bytes"], m["code_lines"]))
    if args.by_dir:
        by = collections.defaultdict(collections.Counter)
        for path, m in per_file.items():
            rest = path[len(args.prefix):]
            by[rest.split("/", 1)[0] if "/" in rest else "(top level)"].update(m)
        print("per directory, heaviest comments first:")
        for top, m in sorted(by.items(), key=lambda kv: -kv[1]["comment_bytes"]):
            print("  %-20s files %4d  bytes %8d  comment bytes %8d (%4.1f%%)  comment lines %6d  "
                  "code lines %6d" % (top, m["files"], m["bytes"], m["comment_bytes"],
                                     100.0 * m["comment_bytes"] / max(1, m["bytes"]),
                                     m["comment_lines"], m["code_lines"]))
    if args.ids:
        fam = collections.Counter()
        for tok, n in ids.items():
            fam[re.sub(r"-[0-9]+[a-z]?$", "", tok)] += n
        print("id-shaped tokens in comments, by family:")
        for f, n in fam.most_common():
            print("  %-16s %d" % (f, n))


if __name__ == "__main__":
    main()
