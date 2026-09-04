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


# ---- session: the seam, and everything else byte-identical ---------------------------------
#
#   (1) session.md at END is START with the moved entries cut out and nothing else;
#   (2) session-restore.md holds exactly the moved entries, byte-identical, in START order,
#       after a preamble whose lines are within 98 bytes; a `## Do not assume` in either
#       register names only laws of that register;
#   (3) every other file that differs from START differs only by: a `// WL-` pointer line
#       whose session.md segment was split by the seam (ids and order kept); the
#       `// Workshop law:` header of a file with such a line; a decision record's link
#       `[WL-SESSION-NN](../workshop/session.md)` re-pathed for a moved id; the router's one
#       row naming both registers; verification.md's one added row. Anything else is a FAIL,
#       and so is a changed file outside those classes (tools/workshop-residues/ aside).
#
#   python3 tools/workshop-residues/prove.py session --repo . --start <commit>

import difflib

SESSION = "agents/workshop/session.md"
RESTORE = "agents/workshop/session-restore.md"


def register_blocks(text):
    lines = text.split("\n")
    idx = [i for i, l in enumerate(lines) if l.startswith("## ")]
    pre = lines[:idx[0]]
    return pre, [(lines[i], lines[i:(idx[k + 1] if k + 1 < len(idx) else len(lines))]) for k, i in enumerate(idx)]


def entry_id(heading):
    m = re.match(r"^## (WL-[A-Z]+-[0-9]+) ", heading)
    return m.group(1) if m else None


def pointer_segments(line):
    m = re.match(r"^([ \t]*)// (WL-.*)$", line)
    if not m:
        return None
    out = []
    for seg in m.group(2).split(";"):
        mm = re.match(r"^\s*(WL-[A-Z]+-[0-9]+(?:\s*,\s*WL-[A-Z]+-[0-9]+)*)\s+--\s+(\S+\.md)\s*$", seg)
        if not mm:
            return None
        out.append(([i.strip() for i in mm.group(1).split(",")], mm.group(2)))
    return m.group(1), out


def prove_session(repo, start, sheet_path):
    failures = []
    seam = collections.OrderedDict()
    with open(sheet_path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip() or line.startswith("id\t"):
                continue
            id_, reg = line.rstrip("\n").split("\t")
            seam[id_] = reg
    moved_ids = [i for i, r in seam.items() if r == RESTORE]
    pre, blocks = register_blocks(git_show(repo, start, SESSION))
    stay_lines, moved_blocks = [], []
    for heading, lines in blocks:
        id_ = entry_id(heading)
        if id_ and seam.get(id_) == RESTORE:
            moved_blocks.append((id_, lines))
        else:
            stay_lines += lines
    # (1)
    if "\n".join(pre + stay_lines) == read(repo, SESSION):
        session_ok = True
    else:
        session_ok = False
        failures.append("(1) %s is not START minus the moved entries" % SESSION)
    # (2)
    rpre, rblocks = register_blocks(read(repo, RESTORE))
    got = [(entry_id(h), l) for h, l in rblocks if entry_id(h)]
    restore_ok = got == moved_blocks
    if not restore_ok:
        failures.append("(2) %s does not hold exactly the moved entries in START order (%s vs %s)"
                        % (RESTORE, [g[0] for g in got], [m[0] for m in moved_blocks]))
    for l in rpre:
        if len(l.encode("utf-8")) > 98:
            failures.append("(2) %s preamble line over 98 bytes: %s" % (RESTORE, l))
    for reg, own in ((SESSION, set(i for i, r in seam.items() if r == SESSION)), (RESTORE, set(moved_ids))):
        _, bl = register_blocks(read(repo, reg))
        for h, lines in bl:
            if h == "## Do not assume":
                for id_ in re.findall(r"WL-SESSION-[0-9]+", "\n".join(lines)):
                    if id_ not in own:
                        failures.append("(2) %s's Do not assume names %s, which is not its law" % (reg, id_))
    sizes = {reg: len(read(repo, reg).encode("utf-8")) for reg in (SESSION, RESTORE)}
    for reg, n in sizes.items():
        if n > REGISTER_BYTES:
            failures.append("(2) %s is %d bytes, over %d" % (reg, n, REGISTER_BYTES))
    # (3)
    changed = subprocess.run(["git", "-C", repo, "diff", "--name-only", start], check=True,
                             capture_output=True).stdout.decode().split()
    untracked = subprocess.run(["git", "-C", repo, "ls-files", "--others", "--exclude-standard"], check=True,
                               capture_output=True).stdout.decode().split()
    classes = collections.Counter()

    def pointer_pair_ok(a, b):
        pa, pb = pointer_segments(a), pointer_segments(b)
        if not pa or not pb or pa[0] != pb[0]:
            return False
        ida = [(i, r) for ids, r in pa[1] for i in ids]
        idb = [(i, r) for ids, r in pb[1] for i in ids]
        want = [(i, RESTORE if (r == SESSION and i in moved_ids) else r) for i, r in ida]
        return sorted(want) == sorted(idb) and [i for i, _ in ida] == [i for i, _ in idb]

    for f in sorted(set(changed) | set(untracked)):
        if f in (SESSION, RESTORE) or f.startswith("tools/workshop-residues/"):
            continue
        if f in untracked:
            failures.append("(3) %s is new" % f)
            continue
        a, b = git_show(repo, start, f).split("\n"), read(repo, f).split("\n")
        pointer_changed = False
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if tag == "equal":
                continue
            if tag == "insert" and f == "agents/verification.md" and j2 - j1 == 1 and b[j1].startswith("| session-restore |"):
                classes["verification row"] += 1
                continue
            if tag != "replace" or i2 - i1 != j2 - j1:
                failures.append("(3) %s: %s of %d/%d lines at %d is not a re-pathing" % (f, tag, i2 - i1, j2 - j1, i1 + 1))
                continue
            for x, y in zip(a[i1:i2], b[j1:j2]):
                if f.endswith((".hpp", ".cpp", ".h", ".ipp", ".cc", ".cxx", ".c")):
                    if pointer_pair_ok(x, y):
                        classes["pointer line"] += 1
                        pointer_changed = True
                        continue
                    if x.startswith("// Workshop law:") and y.startswith("// Workshop law:"):
                        classes["law header"] += 1
                        continue
                elif f.startswith("agents/decisions/"):
                    def relink(m):
                        return "[%s](../workshop/session-restore.md)" % m.group(1) if m.group(1) in moved_ids else m.group(0)
                    if re.sub(r"\[(WL-SESSION-[0-9]+)\]\(\.\./workshop/session\.md\)", relink, x) == y:
                        classes["record link"] += 1
                        continue
                elif f == "agents/workshop.md":
                    if x.replace("[session](workshop/session.md) `WL-SESSION`",
                                 "[session](workshop/session.md) · [session-restore](workshop/session-restore.md) `WL-SESSION`") == y:
                        classes["router row"] += 1
                        continue
                failures.append("(3) %s: a line changed that is no re-pathing:\n      START: %s\n      END:   %s" % (f, x, y))
        if classes["law header"] and not pointer_changed and f.endswith((".hpp", ".cpp")):
            failures.append("(3) %s: its law header changed though no pointer line did" % f)
    print("prove session: (1) %s outside the moved entries %s; (2) %s holds the %d moved entries %s, "
          "%d and %d bytes of %d; (3) %d other files changed -- %s"
          % (SESSION, "byte-identical" if session_ok else "CHANGED", RESTORE, len(moved_blocks),
             "byte-identical" if restore_ok else "DIFFERENT", sizes[SESSION], sizes[RESTORE], REGISTER_BYTES,
             len([f for f in changed if f not in (SESSION, RESTORE) and not f.startswith("tools/workshop-residues/")]),
             ", ".join("%s %d" % kv for kv in sorted(classes.items())) or "nothing"))
    return failures


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("what", choices=["grab-bags", "session"])
    ap.add_argument("--repo", required=True)
    ap.add_argument("--start", required=True)
    ap.add_argument("--sheet", default=None)
    a = ap.parse_args()
    sheet = a.sheet or os.path.join(HERE, "sheet.tsv" if a.what == "grab-bags" else "session.tsv")
    failures = prove_grab_bags(a.repo, a.start, sheet) if a.what == "grab-bags" else prove_session(a.repo, a.start, sheet)
    for f in failures:
        print("FAIL", f)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
