#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE WORKSHOP SPLIT'S PROOF -- the third of sheet -> applier -> proof (AGENTS.md rule o).
#
# It regenerates nothing. It reads the START commit with `git show` and the working tree
# as END, and answers four questions, each with a count, exiting 0 only when all four hold:
#
#   (a) every sheet row's body at END, with leading whitespace and the declared rewrites
#       removed -- `inline` dropped, a declaration-only specifier (static, explicit,
#       virtual, a default argument) dropped from the definition, `WorkshopWeave::`
#       inserted before a member's name, a nested return type qualified -- equals the body
#       at START; and each destination holds exactly its rows' bodies, in START order;
#   (b) the multiset of string and character literals over workshop/*.hpp and *.cpp is
#       byte-identical to START (preprocessor lines aside: an `#include "x"` is a
#       header-name, not a literal);
#   (c) each of the four headers, comments and blank lines stripped, differs from START
#       only by the body-to-prototype replacements and the <windows.h> block moves the
#       sheet names -- a prototype being the declaration up to its body (or its
#       mem-initializer list), `inline` dropped, ending in `;`;
#   (d) in every register under agents/workshop/, everything outside the PROVEN BY
#       paragraphs and the two named LAW lines (WL-FILES-07, WL-FILES-11) is byte-identical
#       to START.
#
#   python3 tools/workshop-split/prove.py --repo . --start <commit>

import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cxx  # noqa: E402

HEADERS = ("workshop/screen.hpp", "workshop/weave.hpp", "workshop/files.hpp",
           "workshop/filesystem_roots.hpp")
CLASS_OF = {"workshop/weave.hpp": "WorkshopWeave"}
LAW_LINES_REWRITTEN = ("WL-FILES-07", "WL-FILES-11")


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
        r["start_line"] = int(r["start_line"])
        r["end_line"] = int(r["end_line"])
    return rows


def functions_of(text, cls):
    decls, _ = cxx.declarations(text)
    out = []
    for d in decls:
        if d.kind != "function" or not d.defined:
            continue
        if cls:
            if not d.scope or d.scope[-1] != cls:
                continue
        elif any(s and s[0].isupper() for s in d.scope):
            continue
        out.append(d)
    return out


def definition_text(text, d):
    line_start = text.rfind("\n", 0, d.start) + 1
    return text[line_start:d.body_close + 1]


def normalize_start(text, d, cls, nested_types, rewrites):
    """Apply the declared rewrites to a START body: what apply.py is allowed to change."""
    line_start = text.rfind("\n", 0, d.start) + 1
    sig = text[line_start:d.body_open]
    body = text[d.body_open:d.body_close + 1]
    po = d.paren_open - line_start
    pc = d.paren_close - line_start
    head, rest = sig[:po], sig[po:]
    m = re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*$", head)
    pre, name = head[:m.start()], head[m.start():]
    for word in ("inline", "static", "explicit", "virtual"):
        if word in rewrites:
            pre = re.sub(r"(?<![A-Za-z0-9_])%s[ \t]+" % word, "", pre, count=1)
    if "nested-return" in rewrites:
        for t in sorted(nested_types, key=len, reverse=True):
            pre = re.sub(r"(?<![A-Za-z0-9_:])%s(?![A-Za-z0-9_])" % re.escape(t), "%s::%s" % (cls, t), pre)
    if "WorkshopWeave::" in rewrites:
        name = "WorkshopWeave::" + name
    params = rest[:pc - po + 1]
    if "default-argument" in rewrites:
        params = drop_defaults(params)
    return squeeze(pre + name + params + rest[pc - po + 1:] + body)


def drop_defaults(params):
    masked, _ = cxx.mask(params)
    out = params
    cuts = []
    depth = 0
    i = 1
    while i < len(masked) - 1:
        c = masked[i]
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        elif c == "=" and depth == 0 and masked[i + 1] != "=" and masked[i - 1] not in "=!<>":
            j = i + 1
            d2 = 0
            while j < len(masked) - 1:
                cj = masked[j]
                if cj in "([{<":
                    d2 += 1
                elif cj in ")]}>":
                    if d2 == 0:
                        break
                    d2 -= 1
                elif cj == "," and d2 == 0:
                    break
                j += 1
            k = i
            while params[k - 1] in " \t":
                k -= 1
            cuts.append((k, j))
            i = j
            continue
        i += 1
    for a, b in reversed(cuts):
        out = out[:a] + out[b:]
    return out


def squeeze(text):
    """Leading whitespace removed from every line; blank lines kept as blank."""
    return "\n".join(l.lstrip(" \t") for l in text.split("\n"))


def strip(text):
    return cxx.strip_comments_and_blanks(text)


def paragraph_mask(text):
    """The register with every PROVEN BY paragraph and the two named LAW lines replaced by
    a marker, so the rest can be compared byte for byte."""
    lines = text.split("\n")
    out = []
    i = 0
    current = None
    while i < len(lines):
        l = lines[i]
        m = re.match(r"^## (WL-[A-Z]+-[0-9]+) ", l)
        if m:
            current = m.group(1)
        if l.startswith("PROVEN BY"):
            out.append("<PROVEN BY>")
            i += 1
            while i < len(lines) and lines[i].strip() and not lines[i].startswith(("WHY", "UNWITNESSED")):
                i += 1
            continue
        if l.startswith("LAW") and current in LAW_LINES_REWRITTEN:
            out.append("<LAW>")
            i += 1
            continue
        out.append(l)
        i += 1
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True)
    ap.add_argument("--start", required=True)
    ap.add_argument("--sheet", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "sheet.tsv"))
    a = ap.parse_args()
    repo, start = a.repo, a.start
    rows = read_sheet(a.sheet)
    failures = []

    # ---- (a) bodies ----
    start_text = {p: git_show(repo, start, p) for p in HEADERS}
    start_funcs = {}
    start_masked = {}
    nested = {}
    for p in HEADERS:
        cls = CLASS_OF.get(p)
        start_funcs[p] = {(d.first_line, d.last_line): d for d in functions_of(start_text[p], cls)}
        decls, start_masked[p] = cxx.declarations(start_text[p])
        nested[p] = {d.name for d in decls if d.kind in ("type", "alias") and d.scope and cls and d.scope[-1] == cls}
    by_dest = collections.defaultdict(list)
    for r in rows:
        if r["kind"] == "function":
            by_dest[r["destination"]].append(r)
    proven = 0
    for dest in sorted(by_dest):
        drows = sorted(by_dest[dest], key=lambda r: (r["file"], r["start_line"]))
        end_text = read(repo, dest)
        end_funcs = [d for d in functions_of(end_text, None) if not any(s and s[0].isupper() and s != "WorkshopWeave" for s in d.scope)]
        # an out-of-line member's scope is read from its qualified name
        if len(end_funcs) != len(drows):
            failures.append("(a) %s holds %d function bodies, the sheet sends it %d" % (dest, len(end_funcs), len(drows)))
        for r, d_end in zip(drows, end_funcs):
            p = r["file"]
            d_start = start_funcs[p].get((r["start_line"], r["end_line"]))
            if d_start is None or d_start.name != r["name"]:
                failures.append("(a) %s:%d %s is not a function START defines" % (p, r["start_line"], r["name"]))
                continue
            want = normalize_start(start_text[p], d_start, CLASS_OF.get(p), nested[p], r["rewrites"].split(","))
            got = squeeze(definition_text(end_text, d_end))
            if want != got:
                failures.append("(a) %s -> %s: body differs (%s)" % (p, dest, r["name"]))
                wl, gl = want.split("\n"), got.split("\n")
                for k, (x, y) in enumerate(zip(wl, gl)):
                    if x != y:
                        failures.append("      START: %s\n      END:   %s" % (x, y))
                        break
            else:
                proven += 1

    # ---- (b) literals ----
    def literals(texts):
        c = collections.Counter()
        for t in texts:
            c.update(cxx.mask(t)[1])
        return c
    start_files = [f for f in git_ls(repo, start, "workshop/") if f.endswith((".hpp", ".cpp"))]
    end_files = sorted(os.path.join("workshop", f) for f in os.listdir(os.path.join(repo, "workshop")) if f.endswith((".hpp", ".cpp")))
    lit_start = literals(git_show(repo, start, f) for f in start_files)
    lit_end = literals(read(repo, f) for f in end_files)
    if lit_start != lit_end:
        gone = lit_start - lit_end
        new = lit_end - lit_start
        failures.append("(b) string literals differ: %d gone, %d new; first gone %r, first new %r" % (
            sum(gone.values()), sum(new.values()), next(iter(gone), None), next(iter(new), None)))
    literal_count = sum(lit_end.values())

    # ---- (c) headers ----
    headers_ok = 0
    for p in HEADERS:
        text = start_text[p]
        lines = text.split("\n")
        edits = []
        for r in rows:
            if r["file"] != p:
                continue
            if r["kind"] == "block":
                edits.append((r["start_line"], r["end_line"], []))
                continue
            d = start_funcs[p][(r["start_line"], r["end_line"])]
            proto = cxx.prototype_text(text, start_masked[p], d)
            edits.append((d.first_line, d.last_line, proto.split("\n")))
        expect = list(lines)
        for a_, b_, repl in sorted(edits, reverse=True):
            expect[a_ - 1:b_] = repl
        want = strip("\n".join(expect))
        got = strip(read(repo, p))
        if want != got:
            failures.append("(c) %s differs from START beyond the sheet's replacements" % p)
            wl, gl = want.split("\n"), got.split("\n")
            for k, (x, y) in enumerate(zip(wl, gl)):
                if x != y:
                    failures.append("      expected: %s\n      END:      %s" % (x, y))
                    break
            if len(wl) != len(gl):
                failures.append("      expected %d code lines, END has %d" % (len(wl), len(gl)))
        else:
            headers_ok += 1

    # ---- (d) registers ----
    regs = [f for f in git_ls(repo, start, "agents/workshop/") if f.endswith(".md")]
    registers_ok = 0
    for reg in regs:
        s = paragraph_mask(git_show(repo, start, reg))
        e = paragraph_mask(read(repo, reg))
        if s != e:
            failures.append("(d) %s changed outside PROVEN BY and the two named LAW lines" % reg)
            sl, el = s.split("\n"), e.split("\n")
            for x, y in zip(sl, el):
                if x != y:
                    failures.append("      START: %s\n      END:   %s" % (x, y))
                    break
        else:
            registers_ok += 1
        size = len(read(repo, reg).encode("utf-8"))
        if size > 16384:
            failures.append("(d) %s is %d bytes, over 16,384" % (reg, size))

    print("prove: (a) %d of %d bodies identical under the declared rewrites; (b) %d string literals, multiset %s; "
          "(c) %d of %d headers differ only by the sheet's replacements; (d) %d of %d registers unchanged outside PROVEN BY and the two LAW lines"
          % (proven, sum(1 for r in rows if r["kind"] == "function"), literal_count,
             "identical" if lit_start == lit_end else "DIFFERENT", headers_ok, len(HEADERS), registers_ok, len(regs)))
    for f in failures:
        print("FAIL", f)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
