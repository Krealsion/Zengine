# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The census of plan codes in a tree: case names that carry one, the documents' quoted citations
# of those names, and the phase ids a current-facing document carries outside its citations.
#
#   python tools/phase-codes/census.py [--repo DIR] [--list cases|citations|ids]

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
    ap.add_argument("--list", choices=("cases", "citations", "ids"))
    a = ap.parse_args()
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


if __name__ == "__main__":
    main()
