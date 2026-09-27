# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The census of plan codes in a tree: case names that carry one, the documents' quoted citations
# of those names, and the phase ids a current-facing document carries outside its citations.
# Also what tests/check_law_register.cmake resolves: every case a document cites by name, and the
# case names that open with a bare label.
#
#   python tools/phase-codes/census.py [--repo DIR] [--list cases|citations|ids|pins|labels]

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cases  # noqa: E402
import codes  # noqa: E402

# What doc_links does not read as current-facing: frozen, generated, vendored or quarried.
DOC_EXCLUDE = re.compile(r"^(build(-[^/]*)?/|cmake-build|_install|\.git/|out/|\.idea/|\.vscode/|"
                         r"\.claude/|docs/history/|reference/)|third_party/")
# A citation is a double-quoted string, which may wrap across lines.
QUOTED = re.compile(r'"([^"\n]*(?:\n[^"\n]*){0,3})"')
WRAP = re.compile(r"[ \t]*\n[ \t]*")
# A case cited by name, as law_register reads one: under agents/ every backticked quote
# `"..."`, and anywhere the word case, subcase or cases before a quote, backticked or not. The
# name may wrap and may hold backticks; it holds no double quote.
CITED_AGENTS = re.compile(r'`"([^"]*)"`')
CITED_WORD = re.compile(r'(?<![A-Za-z])(?:case|subcase|cases)[ \t\n]+`?"([^"]*)"')


def documents(repo):
    out = []
    for root, dirs, files in os.walk(repo):
        rel_root = os.path.relpath(root, repo).replace(os.sep, "/")
        rel_root = "" if rel_root == "." else rel_root + "/"
        dirs[:] = sorted(d for d in dirs if not DOC_EXCLUDE.search(rel_root + d + "/"))
        for f in sorted(files):
            rel = rel_root + f
            if f.endswith(".md") and not DOC_EXCLUDE.search(rel):
                out.append(rel)
    return out


def citations(text, names):
    """[(start, end, name)] for every quoted string in text that is a case name once its wraps
    are read as spaces; start and end cover the quotes."""
    out = []
    for m in QUOTED.finditer(text):
        name = WRAP.sub(" ", m.group(1))
        if name in names:
            out.append((m.start(), m.end(), name))
    return out


def case_citations(rel, text):
    """[(line, name)]: every case the document cites by name, its wraps read as spaces."""
    found = {}
    grammar = [CITED_WORD] + ([CITED_AGENTS] if rel.startswith("agents/") else [])
    for g in grammar:
        for m in g.finditer(text):
            found[m.start(1)] = WRAP.sub(" ", m.group(1))
    return [(text.count("\n", 0, s) + 1, name) for s, name in sorted(found.items())]


def pins(repo):
    """[(doc, line, name)] for every cited case that names no TEST_CASE or SUBCASE."""
    names = {c.name for c in cases.all_cases(repo)}
    out = []
    for doc in documents(repo):
        with open(os.path.join(repo, doc), encoding="utf-8") as f:
            text = f.read()
        out.extend((doc, n, name) for n, name in case_citations(doc, text) if name not in names)
    return out


def measure(repo):
    all_cases = cases.all_cases(repo)
    coded = [c for c in all_cases if codes.case_codes(c.name)]
    law_led = [c for c in all_cases if codes.is_law_led(c.name)]
    coded_names = {c.name for c in coded}
    all_names = {c.name for c in all_cases}
    cites = []          # (doc, line, name)
    ids = []            # (doc, line, id)
    for doc in documents(repo):
        with open(os.path.join(repo, doc), encoding="utf-8") as f:
            text = f.read()
        found = citations(text, all_names)
        blank = list(text)
        for s, e, name in found:
            if name in coded_names:
                cites.append((doc, text.count("\n", 0, s) + 1, name))
            for i in range(s, e):
                if blank[i] != "\n":
                    blank[i] = " "
        rest = "".join(blank)
        for n, line in enumerate(rest.split("\n"), 1):
            for t in codes.document_ids(line):
                ids.append((doc, n, t))
    return all_cases, coded, law_led, cites, ids


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--list", choices=("cases", "citations", "ids", "pins", "labels"))
    a = ap.parse_args()
    if a.list == "pins":
        for doc, line, name in pins(a.repo):
            print(f"{doc}\t{line}\t{name}")
        return
    if a.list == "labels":
        for c in cases.all_cases(a.repo):
            if codes.is_bare_label(c.name):
                print(f"{c.path}\t{c.line}\t{c.macro}\t{c.name}")
        return
    all_cases, coded, law_led, cites, ids = measure(a.repo)
    if a.list == "cases":
        for c in coded:
            print(f"{c.path}\t{c.line}\t{c.macro}\t{c.name}")
        return
    if a.list == "citations":
        for doc, line, name in cites:
            print(f"{doc}\t{line}\t{name}")
        return
    if a.list == "ids":
        for doc, line, t in ids:
            print(f"{doc}\t{line}\t{t}")
        return
    print(f"case names: {len(all_cases)}")
    print(f"  with a plan code: {len(coded)} in {len({c.path for c in coded})} files")
    print(f"  law-led, no code: {len(law_led)}")
    print(f"citations of a coded name: {len(cites)} in {len({d for d, _, _ in cites})} documents,"
          f" {len({n for _, _, n in cites})} names")
    by_root = collections.Counter(d.split("/", 1)[0] if "/" in d else "(root)" for d, _, _ in cites)
    print(f"  by root: {dict(by_root)}")
    print(f"document phase ids outside citations: {len(ids)}")
    by_root = collections.Counter(d.split("/", 1)[0] if "/" in d else "(root)" for d, _, _ in ids)
    print(f"  by root: {dict(by_root)}")
    stale = pins(a.repo)
    print(f"cited cases naming no case: {len(stale)} in {len({d for d, _, _ in stale})} documents")
    print(f"case names opening with a bare label:"
          f" {sum(1 for c in all_cases if codes.is_bare_label(c.name))}")


if __name__ == "__main__":
    main()
