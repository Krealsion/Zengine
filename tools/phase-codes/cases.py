# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# Every TEST_CASE and SUBCASE name under tests/, read as tests/check_law_register.cmake reads it:
# adjacent string literals joined, so a name wrapped over two source lines is one name. Each name
# keeps the spans of the literals that spell it, which is what the applier rewrites.

import os
import re
import sys

# Appended, not prepended: the comment pass has a census.py of its own.
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "comment-pass"))
import lex  # noqa: E402

MACRO = re.compile(r"\b(TEST_CASE|SUBCASE)\s*\(\s*$")
SUFFIXES = (".cpp", ".hpp")
# What law_register does not read, and so what this reader does not either.
EXCLUDED = ("tests/third_party/",)


class Case:
    __slots__ = ("path", "macro", "name", "pieces", "line")

    def __init__(self, path, macro, name, pieces, line):
        self.path = path        # repository-relative, forward slashes
        self.macro = macro      # TEST_CASE or SUBCASE
        self.name = name        # the joined literal text, escapes as written
        self.pieces = pieces    # [(start, end)] of each literal, quotes included
        self.line = line        # 1-based line of the first literal


def test_files(repo):
    out = []
    for root, dirs, files in os.walk(os.path.join(repo, "tests")):
        dirs.sort()
        for f in sorted(files):
            if f.endswith(SUFFIXES):
                rel = os.path.relpath(os.path.join(root, f), repo).replace(os.sep, "/")
                if not rel.startswith(EXCLUDED):
                    out.append(rel)
    return sorted(out)


def read(repo, rel):
    with open(os.path.join(repo, rel), "rb") as f:
        return f.read().decode("utf-8")


def cases_in(rel, text):
    """The cases one file declares, in source order."""
    spans = lex.cxx_spans(text)
    out = []
    for i, (kind, s, e) in enumerate(spans):
        if kind != lex.CODE:
            continue
        m = MACRO.search(text, s, e)
        if not m or m.end() != e:
            continue
        pieces = []
        j = i + 1
        while j < len(spans):
            k, ps, pe = spans[j]
            if k == lex.LITERAL and text[ps] == '"':
                pieces.append((ps, pe))
            elif k == lex.CODE and text[ps:pe].strip() == "":
                pass
            elif k == lex.COMMENT:
                pass
            else:
                break
            j += 1
        if not pieces:
            continue
        name = "".join(text[ps + 1:pe - 1] for ps, pe in pieces)
        line = text.count("\n", 0, pieces[0][0]) + 1
        out.append(Case(rel, m.group(1), name, pieces, line))
    return out


def all_cases(repo):
    out = []
    for rel in test_files(repo):
        out.extend(cases_in(rel, read(repo, rel)))
    return out
