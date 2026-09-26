# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The case map's proof: the third of map -> applier -> proof (AGENTS.md rule o). It regenerates
# nothing. Every file that differs from the start commit must be one of: a C++ or CMake file whose
# code and literals equal the start commit's with the map's case names and messages.tsv's messages
# applied, comments and the whitespace between tokens aside; Markdown; this directory; or a check
# named in CHECKS below, whose changed code lines are printed. Anything else is refused. It then
# asks the tree itself: no quoted citation of an old name is left in a current-facing document,
# and no case name in the tree is an old name the map renamed.
#
#   python tools/phase-codes/prove.py --start <commit>           the proof
#   python tools/phase-codes/prove.py --start <commit> --demo    ...and show what it refuses

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apply  # noqa: E402
import cases  # noqa: E402
import census  # noqa: E402
import lex  # noqa: E402  (on the path through cases)

HERE = os.path.dirname(os.path.abspath(__file__))
MESSAGES = os.path.join(HERE, "messages.tsv")
TOOL_DIR = "tools/phase-codes/"
# The checks this pass teaches to see a plan code, set aside whole with their code lines printed.
CHECKS = ("tests/check_law_register.cmake", "tests/check_doc_links.cmake")


def read_messages():
    """[(file, old literal, new literal)]: each message is its source spelling, quotes included."""
    rows = []
    with open(MESSAGES, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 3:
                sys.exit(f"messages.tsv:{n}: a row is file, old literal, new literal")
            rows.append(tuple(parts))
    return rows


def items(rel, text):
    """The file as a sequence: each code token, and each literal whole; comments dropped."""
    out = []
    for kind, s, e in lex.spans_of(rel, text):
        if kind == lex.CODE:
            out.extend(text[s:e].split())
        elif kind == lex.LITERAL:
            out.append(text[s:e])
    return out


def changed_files(repo, start):
    out = subprocess.run(["git", "-C", repo, "diff", "--name-only", start, "--"],
                         capture_output=True, check=True).stdout.decode("utf-8").split("\n")
    new = subprocess.run(["git", "-C", repo, "ls-files", "--others", "--exclude-standard"],
                         capture_output=True, check=True).stdout.decode("utf-8").split("\n")
    return sorted({f for f in out + new if f})


def read_tree(repo, rel):
    path = os.path.join(repo, rel)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def judge(repo, start, planned, messages, tree=None):
    """([refusals], [notes]). `tree` maps a path to the text to judge in place of the file's."""
    tree = tree or {}
    refusals, notes = [], []
    by_file = {}
    for f, old, new in messages:
        by_file.setdefault(f, []).append((old, new))
    files = sorted(set(changed_files(repo, start)) | set(tree))
    for rel in files:
        end = tree.get(rel, read_tree(repo, rel))
        begin = apply.git_show(repo, start, rel)
        if rel.startswith(TOOL_DIR):
            notes.append(f"tool {rel}")
            continue
        if rel.endswith(".md"):
            continue
        if begin is None or end is None:
            refusals.append(f"{rel}: added or deleted, and it is neither Markdown nor this tool")
            continue
        kind = lex.kind_of(rel)
        if kind is None:
            refusals.append(f"{rel}: changed, and it is not C++, CMake, Markdown or this tool")
            continue
        expected = items(rel, planned.get(rel, begin))
        for old, new in by_file.get(rel, []):
            hits = [i for i, t in enumerate(expected) if t == old]
            if len(hits) != 1:
                refusals.append(f"{rel}: message {old} occurs {len(hits)} times at the start")
                continue
            expected[hits[0]] = new
            notes.append(f"message {rel}: {old} -> {new}")
        got = items(rel, end)
        if expected == got:
            continue
        if rel in CHECKS:
            a, b = set(expected), set(got)
            notes.append(f"check {rel}: {len(b - a)} token(s) added, {len(a - b)} removed")
            continue
        for i, (x, y) in enumerate(zip(expected, got)):
            if x != y:
                refusals.append(f"{rel}: code differs at token {i}: expected {x[:80]!r},"
                                f" found {y[:80]!r}")
                break
        else:
            refusals.append(f"{rel}: {len(expected)} tokens expected, {len(got)} found")
    return refusals, notes


def tree_questions(repo, rows):
    """Refusals the finished tree answers: an old name still cited or still declared."""
    refusals = []
    olds = {old for (_, old) in rows}
    for doc in census.documents(repo):
        text = read_tree(repo, doc)
        for s, _, name in census.citations(text, olds):
            refusals.append(f"{doc}:{text.count(chr(10), 0, s) + 1}: still cites {name!r}")
    for c in cases.all_cases(repo):
        if (c.path, c.name) in rows:
            refusals.append(f"{c.path}:{c.line}: still declares {c.name!r}")
    return refusals


def demo(repo, start, planned, messages):
    """Three edits the proof must refuse, each judged in memory against the finished tree."""
    rel = sorted(r for r in planned if r.startswith("tests/"))[0]
    end = read_tree(repo, rel)
    first = cases.cases_in(rel, end)[0]
    s, e = first.pieces[0][0], first.pieces[-1][1]
    named = {p[0] for c in cases.cases_in(rel, end) for p in c.pieces}
    other = next(ls for k, ls, _ in lex.spans_of(rel, end)
                 if k == lex.LITERAL and end[ls] == '"' and ls not in named)
    trials = {
        "a code edit": end.replace("CHECK(", "CHECK_FALSE(", 1),
        "a case name off the map": end[:s] + '"a name no map row gives"' + end[e:],
        "a literal off the list": end[:other + 1] + "x" + end[other + 1:],
    }
    ok = True
    for what, text in trials.items():
        if text is None or text == end:
            print(f"demo: {what}: could not be made in {rel}")
            ok = False
            continue
        refusals, _ = judge(repo, start, planned, messages, {rel: text})
        caught = any(r.startswith(rel) for r in refusals)
        print(f"demo: {what} in {rel}: {'REFUSED' if caught else 'NOT CAUGHT'}")
        ok = ok and caught
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--start", required=True)
    ap.add_argument("--demo", action="store_true")
    a = ap.parse_args()
    rows = apply.read_map()
    planned, test_report, doc_report = apply.plan(a.repo, a.start, rows)
    messages = read_messages()
    refusals, notes = judge(a.repo, a.start, planned, messages)
    refusals += tree_questions(a.repo, rows)
    for n in notes:
        print("  " + n)
    for r in refusals:
        print("REFUSED " + r)
    changed = changed_files(a.repo, a.start)
    print(f"prove: {len(changed)} files differ from {a.start}; map rows {len(rows)},"
          f" cases {sum(test_report.values())}, citations {sum(doc_report.values())},"
          f" messages {len(messages)}; refusals {len(refusals)}")
    ok = not refusals
    if a.demo:
        ok = demo(a.repo, a.start, planned, messages) and ok
    print("prove: IDENTICAL" if ok else "prove: DIFFERENT")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
