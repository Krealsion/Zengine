# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# The comment pass's proof: the third of map -> applier -> proof (AGENTS.md rule o). It
# regenerates nothing. For every C++ and CMake file in the repository, and the test-population
# manifest -- the start commit's and the working tree's -- it strips comments, collapses
# whitespace, drops blank lines, and asks whether START with the rename map applied to its code
# equals END, line for line, and whether the two carry the same literals. The manifest is also
# read as tests/check_population.cmake reads it, through CMake, since a bracket in a comment can
# hide the data lines below it. The allowed differences are the pass's instruments: the check's
# own file, whose every differing code line is printed; `doc_links`' source-root list, printed;
# and the check's registration line in tests/CMakeLists.txt when START did not have it. Every
# other changed file must be markdown or this directory's. Every law pointer (`// WL-`, `// MW-`)
# and law line (`// Workshop law:`) must also stand where it stood: the same line, in the same
# file, above the same code. A law line may be corrected to name a file that exists in place of
# one that does not, and a law line may be added; both are printed.
#
#   python tools/comment-pass/prove.py --start <commit>                 the proof
#   python tools/comment-pass/prove.py --start <commit> --demo [FILE..] ...and show what it catches

import argparse
import collections
import difflib
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apply  # noqa: E402
import lex  # noqa: E402

CHECK_FILE = "tests/check_source_comments.cmake"
REGISTRATION_FILE = "tests/CMakeLists.txt"
REGISTRATION = "zengine_script_test(source_comments ${CMAKE_CURRENT_SOURCE_DIR}/check_source_comments.cmake)"
DOC_LINKS_FILE = "tests/check_doc_links.cmake"
DOC_LINKS_ROOTS = re.compile(r"set\(ZEN_DOC_SOURCE_ROOTS\b[^)]*\)")
MANIFEST_FILE = "tests/test_population.txt"
POPULATION_CHECK = "tests/check_population.cmake"
# The manifest's reading in tests/check_population.cmake, restated; the line it keys on must
# still be there, or this restatement is stale and the proof says so.
POPULATION_KEY_LINE = 'string(REGEX REPLACE "#.*$" "" line "${line}")'
READ_AS_POPULATION = r'''file(STRINGS "${MANIFEST}" manifest_lines)
foreach(line IN LISTS manifest_lines)
    string(REGEX REPLACE "#.*$" "" line "${line}")
    string(REPLACE "\t" " " line "${line}")
    string(STRIP "${line}" line)
    if(NOT line STREQUAL "")
        message(STATUS "ENTRY ${line}")
    endif()
endforeach()
'''
LAW_LINE = re.compile(r"^\s*// (?:(?:WL|MW)-[A-Z]+-\d+|[A-Z][A-Za-z]* law:)")
LAW_PATH = re.compile(r"^// ([A-Z][A-Za-z]* law:) (\S+)$")
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{2,}")


def git_lines(repo, *args):
    out = subprocess.run(["git", "-C", repo] + list(args), capture_output=True, check=True)
    return [p for p in out.stdout.decode("utf-8").splitlines() if p]


def read_start(repo, commit, paths):
    """{path: text} for every path at the commit, through one `git cat-file --batch`."""
    names = "".join("%s:%s\n" % (commit, p) for p in paths).encode("utf-8")
    out = subprocess.run(["git", "-C", repo, "cat-file", "--batch"], input=names,
                         capture_output=True, check=True).stdout
    texts, i = {}, 0
    for p in paths:
        header_end = out.index(b"\n", i)
        size = int(out[i:header_end].split()[2])
        body = out[header_end + 1:header_end + 1 + size]
        texts[p] = body.decode("utf-8", "replace").replace("\r\n", "\n")
        i = header_end + 1 + size + 1
    return texts


def renamed(path, text, rows):
    """The text with the map's renames applied to its code, never to a comment or literal."""
    if not rows or lex.kind_of(path) != "cxx":
        return text
    parts = []
    for kind, s, e in lex.spans_of(path, text):
        piece = text[s:e]
        if kind == lex.CODE:
            for row in rows:
                piece = apply.token(row["old"]).sub(row["new"], piece)
        parts.append(piece)
    return "".join(parts)


def roots_statement(text):
    """`doc_links`' source-root list, whitespace collapsed, read from its file's code."""
    m = DOC_LINKS_ROOTS.search(lex.without_comments(text, lex.cmake_spans(text)))
    return " ".join(m.group(0).split()) if m else None


def code_form(path, text, rows):
    """(normalized code lines, literals), with the map's renames applied to code only and
    `doc_links`' source-root list set aside."""
    text = renamed(path, text, rows)
    spans = lex.spans_of(path, text)
    if path != DOC_LINKS_FILE:
        return lex.normalized_lines(text, spans), lex.literals(text, spans)
    code = DOC_LINKS_ROOTS.sub("set(ZEN_DOC_SOURCE_ROOTS)", lex.without_comments(text, spans))
    lines = [re.sub(r"[ \t\r\f\v]+", " ", line).strip() for line in code.split("\n")]
    return [line for line in lines if line], lex.literals(text, spans)


def as_population_reads(text):
    """The manifest's entries as tests/check_population.cmake reads them, through CMake itself:
    `file(STRINGS)` splits a line at a non-ASCII byte and joins lines across an open bracket."""
    with tempfile.TemporaryDirectory() as d:
        manifest, script = os.path.join(d, "manifest.txt"), os.path.join(d, "read.cmake")
        with open(manifest, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        with open(script, "w", encoding="utf-8", newline="\n") as f:
            f.write(READ_AS_POPULATION)
        out = subprocess.run(["cmake", "-DMANIFEST=" + manifest.replace("\\", "/"), "-P", script],
                             capture_output=True, check=True).stdout.decode("utf-8", "replace")
    return [line[len("-- ENTRY "):] for line in out.splitlines() if line.startswith("-- ENTRY ")]


def compare(path, start_text, end_text, rows, registration_new):
    """None when equal, else the first difference in words."""
    if path == MANIFEST_FILE:
        a_reads, b_reads = as_population_reads(start_text), as_population_reads(end_text)
        if a_reads != b_reads:
            for k, (x, y) in enumerate(zip(a_reads, b_reads)):
                if x != y:
                    return "as the population check reads it, entry %d: %s  ->  %s" % (k + 1, x[:60], y[:60])
            return "as the population check reads it, %d entries -> %d" % (len(a_reads), len(b_reads))
    a_lines, a_lits = code_form(path, start_text, rows)
    b_lines, b_lits = code_form(path, end_text, [])
    if path == REGISTRATION_FILE and registration_new:
        if b_lines.count(REGISTRATION) != 1:
            return "the check's registration line is not there exactly once"
        b_lines.remove(REGISTRATION)
    if a_lits != b_lits:
        for k, (x, y) in enumerate(zip(a_lits, b_lits)):
            if x != y:
                return "literal %d: %s -> %s" % (k, x[:60], y[:60])
        return "literal count %d -> %d" % (len(a_lits), len(b_lits))
    if a_lines != b_lines:
        for k, (x, y) in enumerate(zip(a_lines, b_lines)):
            if x != y:
                return "code line %d: %s  ->  %s" % (k + 1, x[:70], y[:70])
        return "code line count %d -> %d" % (len(a_lines), len(b_lines))
    return None


def law_lines(path, text, rows):
    """Counter of (path, law line, the first code line after it), renames applied to code."""
    if lex.kind_of(path) != "cxx":
        return collections.Counter()
    text = renamed(path, text, rows)
    classes = [c for c, _, _ in lex.line_bytes(text, lex.spans_of(path, text))]
    lines = text.split("\n")
    out = collections.Counter()
    for k, line in enumerate(lines[:len(classes)]):
        if classes[k] == "comment" and LAW_LINE.match(line):
            nxt = next((re.sub(r"\s+", " ", lines[j]).strip() for j in range(k + 1, len(classes))
                        if classes[j] == "code"), "")
            out[(path, line.strip(), nxt)] += 1
    return out


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--start", required=True)
    ap.add_argument("--map", default=apply.MAP)
    ap.add_argument("--demo", nargs="*", metavar="FILE",
                    help="show what the comparison catches, on each FILE (default workshop/setup.hpp)")
    args = ap.parse_args()
    repo = args.repo
    rows = apply.read_map(os.path.join(repo, args.map))
    start = {p for p in git_lines(repo, "ls-tree", "-r", "--name-only", args.start)
             if lex.kind_of(p)}
    end = {p for p in git_lines(repo, "ls-files") + git_lines(
        repo, "ls-files", "--others", "--exclude-standard") if lex.kind_of(p)}
    end = {p for p in end if os.path.exists(os.path.join(repo, p))}
    start_text = read_start(repo, args.start, sorted(start))
    registration_new = REGISTRATION_FILE in start and \
        REGISTRATION not in code_form(REGISTRATION_FILE, start_text[REGISTRATION_FILE], [])[0]
    failures = []
    set_aside = []
    for p in sorted(start - end):
        failures.append("%s: removed" % p)
    for p in sorted(end - start):
        if p != CHECK_FILE:
            failures.append("%s: added" % p)
    if CHECK_FILE not in end:
        failures.append("%s: the check is missing" % CHECK_FILE)
    changed = set(git_lines(repo, "diff", "--name-only", args.start)) | set(
        git_lines(repo, "ls-files", "--others", "--exclude-standard"))
    for p in sorted(changed):
        if not lex.kind_of(p) and not p.endswith(".md") and not p.startswith(apply.TOOLS):
            failures.append("%s: changed, and it is neither code this proof reads, markdown nor "
                            "this directory's" % p)
    if MANIFEST_FILE in end:
        with open(os.path.join(repo, POPULATION_CHECK), encoding="utf-8") as f:
            if POPULATION_KEY_LINE not in f.read():
                failures.append("%s: its manifest reading changed; restate it here" % POPULATION_CHECK)
    compared = 0
    laws_start, laws_end = collections.Counter(), collections.Counter()
    for p in sorted(start & end):
        with open(os.path.join(repo, p), encoding="utf-8", newline="") as f:
            end_text = f.read().replace("\r\n", "\n")
        laws_start += law_lines(p, start_text[p], rows)
        laws_end += law_lines(p, end_text, [])
        if p == CHECK_FILE:
            a, _ = code_form(p, start_text[p], [])
            b, _ = code_form(p, end_text, [])
            set_aside = [d for d in difflib.unified_diff(a, b, lineterm="", n=0)
                         if d[:1] in "+-" and d[:3] not in ("+++", "---")]
            continue
        compared += 1
        why = compare(p, start_text[p], end_text, rows, registration_new)
        if why:
            failures.append("%s: %s" % (p, why))
    dropped, added = laws_start - laws_end, laws_end - laws_start
    corrected = []
    for key in sorted(dropped):
        m = LAW_PATH.match(key[1])
        for new in sorted(added):
            n = LAW_PATH.match(new[1])
            if m and n and new[0] == key[0] and new[2] == key[2] and n.group(1) == m.group(1) and \
                    not os.path.exists(os.path.join(repo, m.group(2))) and \
                    os.path.exists(os.path.join(repo, n.group(2))):
                corrected.append((key, new))
                dropped[key] -= 1
                added[new] -= 1
                break
    for key in sorted(+dropped):
        failures.append("%s: law line dropped or moved: %s (above: %s)" % (key[0], key[1][:70], key[2][:50]))
    print("prove: %d C++ and CMake files compared, START with %d renames against the working "
          "tree; %d law pointers and law lines at START, each still above the same code" % (
              compared, len(rows), sum(laws_start.values())))
    for key, new in corrected:
        print("prove: law line corrected in %s: %s -> %s (the file it named does not exist)" % (
            key[0], LAW_PATH.match(key[1]).group(2), LAW_PATH.match(new[1]).group(2)))
    for key in sorted(+added):
        print("prove: law line added in %s: %s (above: %s)" % (key[0], key[1][:70], key[2][:50]))
    if set_aside:
        print("prove: set aside, the check's own file %s: %d code lines differ" % (CHECK_FILE, len(set_aside)))
        for d in set_aside:
            print("    " + d)
    else:
        print("prove: set aside, the check's own file %s: %s" % (
            CHECK_FILE, "new" if CHECK_FILE not in start else "no code line differs"))
    if DOC_LINKS_FILE in start and DOC_LINKS_FILE in end:
        with open(os.path.join(repo, DOC_LINKS_FILE), encoding="utf-8", newline="") as f:
            before, after = roots_statement(start_text[DOC_LINKS_FILE]), roots_statement(f.read())
        print("prove: set aside, doc_links' source roots in %s: %s" % (
            DOC_LINKS_FILE, "unchanged" if before == after else "\n    was: %s\n    now: %s" % (before, after)))
    if MANIFEST_FILE in start:
        print("prove: %s read as the population check reads it, through CMake: %d entries at START" % (
            MANIFEST_FILE, len(as_population_reads(start_text[MANIFEST_FILE]))))
    if registration_new:
        print("prove: set aside, the check's registration line in %s" % REGISTRATION_FILE)
    for f in failures:
        print("  DIFFERS  " + f)
    print("prove: %s" % ("identical" if not failures else "%d differences" % len(failures)))
    status = 0 if not failures else 1
    if args.demo is not None:
        for path in args.demo or ["workshop/setup.hpp"]:
            status |= demo(repo, start_text, rows, path, registration_new)
    return status


# What each demo edit must do: be caught (True) or pass as comment-only (False).
DEMO_EDITS = (("one-token change", True), ("literal change", True), ("comment-only change", False),
              ("an open bracket in a manifest comment", True),
              ("a non-ASCII byte in a manifest comment", True))


def mutations(path, text):
    """Edits to a file, each in its middle: one code token, one literal, one comment; and in the
    manifest, the two comment edits only its CMake reading sees."""
    spans = lex.spans_of(path, text)
    out = {}
    if path == MANIFEST_FILE:
        lines = text.split("\n")
        classes = [c for c, _, _ in lex.line_bytes(text, spans)]
        above = [k for k in range(1, len(classes)) if classes[k] == "code" and classes[k - 1] == "comment"]
        if above:
            k = above[len(above) // 2] - 1
            out["an open bracket in a manifest comment"] = "\n".join(
                lines[:k] + [lines[k] + " [ still open"] + lines[k + 1:])
            out["a non-ASCII byte in a manifest comment"] = "\n".join(
                lines[:k] + [lines[k] + " — then more"] + lines[k + 1:])
    idents = []
    for kind, s, e in spans:
        if kind != lex.CODE:
            continue
        for m in IDENT.finditer(text, s, e):
            line_start = text.rfind("\n", 0, m.start()) + 1
            if not text[line_start:m.start()].lstrip().startswith("#"):
                idents.append(m)
    if idents:
        m = idents[len(idents) // 2]
        out["one-token change"] = text[:m.end()] + "z" + text[m.end():]
    lits = [(s, e) for kind, s, e in spans if kind == lex.LITERAL and text[s] == '"' and e - s >= 4]
    if lits:
        s, e = lits[len(lits) // 2]
        c = "x" if text[s + 1] != "x" else "y"
        out["literal change"] = text[:s + 1] + c + text[s + 2:]
    coms = [(s, e) for kind, s, e in spans if kind == lex.COMMENT and re.search(r"[A-Za-z]", text[s + 2:e])]
    if coms:
        s, e = coms[len(coms) // 2]
        k = s + 2 + re.search(r"[A-Za-z]", text[s + 2:e]).start()
        c = "q" if text[k] != "q" else "j"
        out["comment-only change"] = text[:k] + c + text[k + 1:]
    return out


def demo(repo, start_text, rows, path, registration_new):
    """Edits to one END file, in memory: a code token and a literal must be caught, a
    comment-only edit must not be, and in the manifest so must the two comment edits that
    change what the population check reads."""
    if path not in start_text:
        print("demo: %s is not a START file" % path)
        return 1
    with open(os.path.join(repo, path), encoding="utf-8") as f:
        end_text = f.read()
    edits = mutations(path, end_text)
    ok = tried = 0
    for what, want in DEMO_EDITS:
        if what not in edits:
            if "manifest" not in what or path == MANIFEST_FILE:
                print("demo: %s: %s has nothing to change" % (what, path))
            continue
        tried += 1
        why = compare(path, start_text[path], edits[what], rows, registration_new)
        caught = why is not None
        print("demo: %s in %s: %s%s" % (what, path, "caught" if caught else "not a difference",
                                       (" -- " + why) if why else ""))
        ok += caught == want
    print("demo: %d of %d as expected in %s" % (ok, tried, path))
    return 0 if ok == tried and tried >= 3 else 1


if __name__ == "__main__":
    sys.exit(main())
