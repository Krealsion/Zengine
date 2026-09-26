# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The case map's applier: the second of map -> applier -> proof (AGENTS.md rule o). It reads
# map.tsv and, for every file the map owns, writes the start commit's text with the map applied:
# each renamed TEST_CASE or SUBCASE literal in tests/, and each quoted citation of an old name in
# a current-facing Markdown file. It regenerates from the start commit, so a rerun after a row is
# struck converges; commits made after the applier's own are replayed on top of a rerun.
#
#   python tools/phase-codes/apply.py --start <commit>            write the working tree
#   python tools/phase-codes/apply.py --start <commit> --dry-run  say what would change

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cases  # noqa: E402
import census  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
MAP = os.path.join(HERE, "map.tsv")
CODE_WIDTH = 100
DOC_WIDTH = 98


def read_map(path=MAP):
    """{(file, old): new}, struck rows (a leading `#`) left out."""
    rows = {}
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 4 or not parts[2] or parts[1] == parts[2]:
                sys.exit(f"map.tsv:{n}: a row is file, old, new, kind, and new differs from old")
            key = (parts[0], parts[1])
            if key in rows:
                sys.exit(f"map.tsv:{n}: {key} has a second row")
            rows[key] = parts[2]
    return rows


def git_show(repo, start, rel):
    out = subprocess.run(["git", "-C", repo, "show", f"{start}:{rel}"], capture_output=True)
    if out.returncode != 0:
        return None
    return out.stdout.decode("utf-8")


def git_files(repo, start):
    out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "--name-only", start],
                         capture_output=True, check=True)
    return out.stdout.decode("utf-8").split("\n")


def width(s):
    """A line's width in bytes, as the registers' wrap rule measures it."""
    return len(s.encode("utf-8"))


def wrap(words, first, rest, last_extra=0):
    """Greedy lines from words: the first holds `first` bytes, the others `rest`; the last line
    also carries `last_extra` bytes after it, and a word moves down until it does."""
    lines, cur = [], ""
    for w in words:
        cap = first if not lines else rest
        cand = w if not cur else cur + " " + w
        if cur and width(cand) > cap:
            lines.append(cur)
            cur = w
        else:
            cur = cand
    lines.append(cur)
    while True:
        cap = first if len(lines) == 1 else rest
        if width(lines[-1]) + last_extra <= cap or " " not in lines[-1]:
            break
        head, tail = lines[-1].rsplit(" ", 1)
        lines[-1] = head
        lines.append(tail)
    # A lone word moved down takes two more with it, when they fit, so no line is an orphan.
    for _ in range(2):
        if len(lines) < 2 or lines[-1].count(" ") >= 2 or lines[-2].count(" ") < 4:
            break
        head, tail = lines[-2].rsplit(" ", 1)
        cand = tail + " " + lines[-1]
        if width(cand) + last_extra > rest:
            break
        lines[-2], lines[-1] = head, cand
    return lines


def literal_text(text, case, new):
    """The source text that replaces a case's literals, from its first quote to its last."""
    start, end = case.pieces[0][0], case.pieces[-1][1]
    line_start = text.rfind("\n", 0, start) + 1
    line_end = text.find("\n", end)
    line_end = len(text) if line_end == -1 else line_end
    col = start - line_start
    suffix = len(text[end:line_end])
    if col + width(new) + 2 + suffix <= CODE_WIDTH:
        return f'"{new}"'
    if len(case.pieces) > 1:
        second = case.pieces[1][0]
        indent = text[text.rfind("\n", 0, second) + 1:second]
    else:
        indent = " " * col
    # Each piece but the last keeps the space that ends its words, as the source spells a wrap.
    lines = wrap(new.split(" "), CODE_WIDTH - col - 3, CODE_WIDTH - len(indent) - 3, suffix - 1)
    pieces = [f'"{ln} "' for ln in lines[:-1]] + [f'"{lines[-1]}"']
    return ("\n" + indent).join(pieces)


def apply_tests(text, rel, rows, report):
    found = cases.cases_in(rel, text)
    edits = []
    for c in found:
        new = rows.get((rel, c.name))
        if new is not None:
            report.setdefault((rel, c.name), 0)
            report[(rel, c.name)] += 1
            between = text[c.pieces[0][0]:c.pieces[-1][1]]
            if "//" in re.sub(r'"(\\.|[^"\\])*"', "", between):
                sys.exit(f"{rel}:{c.line}: a comment sits between the literals of a renamed case")
            edits.append((c.pieces[0][0], c.pieces[-1][1], literal_text(text, c, new)))
    for s, e, repl in sorted(edits, reverse=True):
        text = text[:s] + repl + text[e:]
    return text


def apply_document(text, by_old, report):
    found = census.citations(text, set(by_old))
    for s, e, old in sorted(found, reverse=True):
        new = by_old[old]
        report.setdefault(old, 0)
        report[old] += 1
        quoted = text[s:e]
        if "\n" not in quoted:
            text = text[:s] + f'"{new}"' + text[e:]
            continue
        # A wrapped citation: rewrap from its opening quote to the end of the line it closes on,
        # continuation lines at the indent the source gave them.
        line_start = text.rfind("\n", 0, s) + 1
        line_end = text.find("\n", e)
        line_end = len(text) if line_end == -1 else line_end
        cont = text[text.rfind("\n", 0, e) + 1:]
        indent = cont[:len(cont) - len(cont.lstrip(" "))]
        body = f'"{new}"' + text[e:line_end]
        lines = wrap(body.split(" "), DOC_WIDTH - width(text[line_start:s]), DOC_WIDTH - len(indent))
        text = text[:s] + ("\n" + indent).join(lines) + text[line_end:]
    return text


def plan(repo, start, rows):
    """{rel: new text} for every file the map changes, and the per-row counts."""
    out = {}
    test_report, doc_report = {}, {}
    for rel in sorted({f for f, _ in rows}):
        text = git_show(repo, start, rel)
        if text is None:
            sys.exit(f"map.tsv names {rel}, which the start commit does not hold")
        new = apply_tests(text, rel, rows, test_report)
        if new != text:
            out[rel] = new
    stale = [k for k in rows if k not in test_report]
    if stale:
        sys.exit("map rows naming no case at the start commit:\n" +
                 "\n".join(f"  {f}\t{o}" for f, o in stale))
    by_old = {old: new for (_, old), new in rows.items()}
    for rel in git_files(repo, start):
        if not rel.endswith(".md") or census.DOC_EXCLUDE.search(rel):
            continue
        text = git_show(repo, start, rel)
        new = apply_document(text, by_old, doc_report)
        if new != text:
            out[rel] = new
    return out, test_report, doc_report


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--start", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    rows = read_map()
    out, test_report, doc_report = plan(a.repo, a.start, rows)
    twice = [k for k, n in test_report.items() if n > 1]
    tests = [r for r in out if r.startswith("tests/")]
    docs = [r for r in out if not r.startswith("tests/")]
    print(f"rows {len(rows)}; cases renamed {sum(test_report.values())} in {len(tests)} files"
          f" (rows naming more than one case: {len(twice)}); citations followed"
          f" {sum(doc_report.values())} of {len(doc_report)} names in {len(docs)} documents")
    if a.dry_run:
        for rel in sorted(out):
            print("  would write", rel)
        return
    for rel, text in out.items():
        with open(os.path.join(a.repo, rel), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)


if __name__ == "__main__":
    main()
