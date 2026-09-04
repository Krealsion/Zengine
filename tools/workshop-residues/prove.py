#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE WORKSHOP RESIDUES' PROOF -- the third of sheet -> applier -> proof (AGENTS.md rule o).
#
# It regenerates nothing. It reads the START commit with `git show` and the working tree as
# END, and answers, each with a count, exiting 0 only when every one holds:
#
#   grab-bags
#   (a) every sheet row's body at END, leading whitespace aside, is byte-identical to the
#       body at START, its `// WL-` pointer group travelled with it, and each destination
#       holds exactly its rows' bodies in START order; each source .cpp at END holds exactly
#       the START functions the sheet did not move, in START order, bodies identical;
#   (b) the multiset of string and character literals over workshop/*.hpp and *.cpp is
#       byte-identical to START (preprocessor lines aside);
#   (c) each header, comments and blank lines stripped, is byte-identical to START -- the
#       banner is a comment and the prototypes did not move;
#   (d) in every register under agents/workshop/, everything outside the PROVEN BY
#       paragraphs is byte-identical to START, and every register is within 16,384 bytes;
#   and no file this phase wrote is over 1,500 lines, and the logic target lists every
#   destination.
#
#   python3 tools/workshop-residues/prove.py grab-bags --repo . --start <commit>

import argparse
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "workshop-split"))
import cxx  # noqa: E402

LINE_CEILING = 1500
REGISTER_BYTES = 16384


def git_show(repo, commit, path):
    return subprocess.run(["git", "-C", repo, "show", "%s:%s" % (commit, path)], check=True,
                          capture_output=True).stdout.decode("utf-8")


def git_ls(repo, commit, prefix):
    out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "--name-only", commit, prefix], check=True,
                         capture_output=True).stdout.decode("utf-8").split()
    return sorted(out)


def read(repo, rel):
    with open(os.path.join(repo, rel), encoding="utf-8") as f:
        return f.read()


def read_sheet(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        header = None
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.rstrip("\n").split("\t")
            if header is None:
                header = parts
                continue
            rows.append(dict(zip(header, parts)))
    for r in rows:
        for c in ("pointer_first", "start_line", "end_line"):
            r[c] = int(r[c])
    return rows


def functions_of(text):
    decls, _ = cxx.declarations(text)
    out = [d for d in decls if d.kind == "function" and d.defined
           and not any(s and s[0].isupper() for s in d.scope)]
    lines = text.split("\n")
    code = [n for n, _ in cxx.code_lines(text)]
    import bisect
    for d in out:
        k = bisect.bisect_left(code, d.first_line)
        prev = code[k - 1] if k > 0 else 0
        d.pointer_lines = cxx.pointer_lines_between(lines, prev, d.first_line)
        d.pointer_ids = re.findall(r"WL-[A-Z]+-[0-9]+", "\n".join(lines[i - 1] for i in d.pointer_lines))
    return out


def body_text(text, d):
    line_start = text.rfind("\n", 0, d.start) + 1
    return squeeze(text[line_start:d.body_close + 1])


def squeeze(text):
    return "\n".join(l.lstrip(" \t") for l in text.split("\n"))


def paragraph_mask(text):
    """The register with every PROVEN BY paragraph replaced by a marker."""
    lines = text.split("\n")
    out = []
    i = 0
    while i < len(lines):
        l = lines[i]
        if l.startswith("PROVEN BY"):
            out.append("<PROVEN BY>")
            i += 1
            while i < len(lines) and lines[i].strip() and not lines[i].startswith(("WHY", "UNWITNESSED")):
                i += 1
            continue
        out.append(l)
        i += 1
    return "\n".join(out)


def prove_grab_bags(repo, start, sheet_path):
    rows = read_sheet(sheet_path)
    failures = []
    sources = sorted({r["source"] for r in rows})
    start_text = {s: git_show(repo, start, s) for s in sources}
    start_funcs = {s: functions_of(start_text[s]) for s in sources}

    # ---- (a) bodies, pointer groups, destinations, and what stays ----
    proven = 0
    by_dest = collections.defaultdict(list)
    moved_keys = collections.defaultdict(set)
    for r in rows:
        by_dest[r["destination"]].append(r)
        moved_keys[r["source"]].add((r["start_line"], r["end_line"]))
    for dest in sorted(by_dest):
        drows = sorted(by_dest[dest], key=lambda r: (r["source"], r["start_line"]))
        if not os.path.exists(os.path.join(repo, dest)):
            failures.append("(a) %s does not exist" % dest)
            continue
        end_text = read(repo, dest)
        end_funcs = functions_of(end_text)
        if len(end_funcs) != len(drows):
            failures.append("(a) %s holds %d function bodies, the sheet sends it %d" % (dest, len(end_funcs), len(drows)))
        for r, d_end in zip(drows, end_funcs):
            at = {(d.first_line, d.last_line): d for d in start_funcs[r["source"]]}
            d_start = at.get((r["start_line"], r["end_line"]))
            if d_start is None or d_start.name != r["name"]:
                failures.append("(a) %s:%d %s is not a function START defines" % (r["source"], r["start_line"], r["name"]))
                continue
            want = body_text(start_text[r["source"]], d_start)
            got = body_text(end_text, d_end)
            if want != got:
                failures.append("(a) %s -> %s: body differs (%s)" % (r["source"], dest, r["name"]))
                for x, y in zip(want.split("\n"), got.split("\n")):
                    if x != y:
                        failures.append("      START: %s\n      END:   %s" % (x, y))
                        break
                continue
            if d_start.pointer_ids != d_end.pointer_ids or r["pointer_ids"] != ",".join(d_end.pointer_ids):
                failures.append("(a) %s -> %s: the pointer group above %s changed (%s -> %s)"
                                % (r["source"], dest, r["name"], ",".join(d_start.pointer_ids), ",".join(d_end.pointer_ids)))
                continue
            proven += 1
    stayed = 0
    for s in sources:
        keep = [d for d in start_funcs[s] if (d.first_line, d.last_line) not in moved_keys[s]]
        end_text = read(repo, s)
        end_funcs = functions_of(end_text)
        if [d.name for d in keep] != [d.name for d in end_funcs]:
            failures.append("(a) %s at END holds %s; START minus the sheet is %s"
                            % (s, [d.name for d in end_funcs], [d.name for d in keep]))
            continue
        for a, b in zip(keep, end_funcs):
            if body_text(start_text[s], a) != body_text(end_text, b) or a.pointer_ids != b.pointer_ids:
                failures.append("(a) %s: %s changed though the sheet did not move it" % (s, a.name))
            else:
                stayed += 1

    # ---- (b) literals ----
    def literals(texts):
        c = collections.Counter()
        for t in texts:
            c.update(cxx.mask(t)[1])
        return c
    start_files = [f for f in git_ls(repo, start, "workshop/") if f.endswith((".hpp", ".cpp"))]
    end_files = sorted(os.path.join("workshop", f) for f in os.listdir(os.path.join(repo, "workshop"))
                       if f.endswith((".hpp", ".cpp")))
    lit_start = literals(git_show(repo, start, f) for f in start_files)
    lit_end = literals(read(repo, f) for f in end_files)
    if lit_start != lit_end:
        gone, new = lit_start - lit_end, lit_end - lit_start
        failures.append("(b) string literals differ: %d gone, %d new; first gone %r, first new %r"
                        % (sum(gone.values()), sum(new.values()), next(iter(gone), None), next(iter(new), None)))

    # ---- (c) headers ----
    headers = sorted({"workshop/%s.hpp" % os.path.basename(s).split("_")[0] for s in sources})
    headers_ok = 0
    for h in headers:
        if cxx.strip_comments_and_blanks(git_show(repo, start, h)) == cxx.strip_comments_and_blanks(read(repo, h)):
            headers_ok += 1
        else:
            failures.append("(c) %s differs from START in its code, not only its comments" % h)

    # ---- (d) registers ----
    regs = [f for f in git_ls(repo, start, "agents/workshop/") if f.endswith(".md")]
    registers_ok = 0
    for reg in regs:
        s_, e_ = paragraph_mask(git_show(repo, start, reg)), paragraph_mask(read(repo, reg))
        if s_ != e_:
            failures.append("(d) %s changed outside PROVEN BY" % reg)
            for x, y in zip(s_.split("\n"), e_.split("\n")):
                if x != y:
                    failures.append("      START: %s\n      END:   %s" % (x, y))
                    break
        else:
            registers_ok += 1
        size = len(read(repo, reg).encode("utf-8"))
        if size > REGISTER_BYTES:
            failures.append("(d) %s is %d bytes, over %d" % (reg, size, REGISTER_BYTES))

    # ---- the ceiling (over the files this phase wrote) and the target ----
    longest = max((read(repo, f).count("\n"), f) for f in sorted(set(by_dest) | set(sources)))
    if longest[0] > LINE_CEILING:
        failures.append("%s is %d lines, over %d" % (longest[1], longest[0], LINE_CEILING))
    cm = read(repo, "workshop/CMakeLists.txt")
    a = cm.index("add_library(zengine-workshop-logic STATIC\n")
    listed = set(cm[a:cm.index("\n)", a)].split()[1:])
    for dest in sorted(by_dest):
        if os.path.basename(dest) not in listed:
            failures.append("workshop/CMakeLists.txt does not list %s in the logic target" % dest)

    print("prove grab-bags: (a) %d of %d bodies identical and their pointers with them, %d stayed put in %d sources; "
          "(b) %d string literals, multiset %s; (c) %d of %d headers identical in code; "
          "(d) %d of %d registers unchanged outside PROVEN BY; longest file %s at %d lines"
          % (proven, len(rows), stayed, len(sources), sum(lit_end.values()),
             "identical" if lit_start == lit_end else "DIFFERENT", headers_ok, len(headers),
             registers_ok, len(regs), longest[1], longest[0]))
    return failures


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("what", choices=["grab-bags"])
    ap.add_argument("--repo", required=True)
    ap.add_argument("--start", required=True)
    ap.add_argument("--sheet", default=os.path.join(HERE, "sheet.tsv"))
    a = ap.parse_args()
    failures = prove_grab_bags(a.repo, a.start, a.sheet)
    for f in failures:
        print("FAIL", f)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
