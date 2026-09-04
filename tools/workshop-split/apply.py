#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
#
# THE WORKSHOP SPLIT'S APPLIER -- sheet -> applier -> proof (AGENTS.md rule o).
#
# What it regenerates, and from which commit: every function body that `sheet.tsv` names
# is cut out of `workshop/screen.hpp`, `workshop/weave.hpp`, `workshop/files.hpp` or
# `workshop/filesystem_roots.hpp` AS THEY ARE AT THE START COMMIT (read with `git show`,
# never from the working tree, so a rerun converges) and written as an out-of-line
# definition into the `workshop/<subject>.cpp` the sheet's `destination` column names; the
# declaration stays in the header as a prototype. The `// WL-` pointer group above a moved
# body travels with the body (rule t); a prototype carries none. Every PROVEN BY paragraph
# in `agents/workshop/*.md` that names one of the four headers is re-pathed identifier by
# identifier to the file that now holds the code, and re-wrapped at 98 bytes from the first
# line the change reached (rule i). Two LAW lines that said "one header" are rewritten to
# the one file that now asks (WL-FILES-07, WL-FILES-11). Each touched file's
# `// Workshop law:` header is regenerated from the pointers it holds, and the logic
# target's source list in `workshop/CMakeLists.txt` is rewritten between its two markers.
#
#   seed   python3 tools/workshop-split/apply.py seed  --repo . --start <commit>
#          writes tools/workshop-split/sheet.tsv from the file plan below: one row per
#          moved function (and one per moved <windows.h> block), which is the review.
#   apply  python3 tools/workshop-split/apply.py apply --repo . --start <commit>
#          reads the sheet and writes the END files into the working tree.
#   budget python3 tools/workshop-split/apply.py budget --repo . --start <commit>
#          reports every register's byte count after re-pathing, before anything is
#          applied (the 16,384-byte budget is a constraint on the plan).
#
# The three declared rewrites of a moved body, and the fourth this tree needed: `inline` is
# dropped; a declaration-only specifier (`static`, `explicit`, `virtual`, a default
# argument) is dropped from the definition; `WorkshopWeave::` is inserted before a member's
# name; and a member whose return type is a type nested in the class has that type
# qualified `WorkshopWeave::T`, because the return type of an out-of-line definition is
# read outside the class scope. prove.py checks each body against START under exactly
# these. Everything else is byte-for-byte the START text, leading whitespace aside.

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
INDENT_OF = {"workshop/weave.hpp": "    "}
WORKSHOP_LAW = re.compile(r"^// Workshop law: .*$", re.M)
SPDX = "// SPDX-License-Identifier: MPL-2.0\n// Copyright (c) 2026 Joshua DeMoss\n"

# ---- THE FILE PLAN: subject, not register ------------------------------------------------
#
# One .cpp per run of consecutive section banners (the `// ----` lines at the scope's
# indent) whose pointers' registers share a witnessing suite. Each entry is the START line
# of the first banner of a run (line 1 for the run before the first banner) and the subject
# the banners name, in the code's own words. A run with no movable body makes no file.
PLAN = {
    "workshop/screen.hpp": [
        (359, "chrome", "the chrome a pane wears, the authored intent projected onto this screen, and placement spent on the pointer"),
        (830, "arrange", "pane management: what a maker is arranging, and how"),
        (1400, "bindings", "spelling the effective bindings"),
        (1760, "gestures", "reading past the ellipsis, the maker's gestures over one session, the size a hand asked for, the one resize affordance, where a pointer is, and what the OBJECTS panel can show"),
        (2396, "terminal", "rendering one participant's record, the editable line, and the completion list"),
        (2829, "pane_state", "the dynamic panels painted, what state one pane is in, a pane's geometry in the face's own language, and a surface sized by what it says"),
        (3650, "hotkeys", "the full hotkey view"),
        (3826, "attention", "what is true right now, projected, and what can I do with this, presented"),
        (4265, "info", "the Info panel's body, its action controls, and one windowed list's rows"),
        (5013, "external", "an external pane's body"),
        (5206, "editor", "the source editor's pane"),
        (5448, "browser", "the project browser, presented, and which revealable row the pointer is on"),
        (5877, "layouts", "the setup line, the setup slot, the layout tabs, and the layouts pane with the bottom band"),
        (6423, "pane_editor", "a maker-made pane, presented, and the pane editor"),
    ],
    "workshop/weave.hpp": [
        (1, "handlers", "the startup files, the host's conditions, and the surface handlers"),
        (673, "pointer", "the contextual-action surface and the pointer"),
        (1893, "seam", "the external pane seam"),
        (2463, "terminal", "the terminal overlay"),
        (2993, "panels", "the dynamic panels"),
        (3157, "session", "the setup, the layout shelf and the last session"),
        (3671, "arrange", "pane management, and the pointer inside arrangement"),
        (4522, "editor", "the source editor and the filesystem browser"),
        (5048, "pane_editor", "the pane editor and the pane creator"),
        (5709, "save", "save and open"),
    ],
    "workshop/files.hpp": [(1, "files", "whether a directory leaves the tree, asked of the host")],
    "workshop/filesystem_roots.hpp": [(1, "filesystem_roots", "the filesystem roots this host reports")],
}
# Only these bodies move out of the two small headers ("the Win32 half"); every other
# function there stays inline.
ONLY = {
    "workshop/files.hpp": {"leaves_the_tree"},
    "workshop/filesystem_roots.hpp": {"host_filesystem_roots"},
}

# The two LAW lines rewritten (each at most 210 bytes; the second is exactly 210).
LAW_REWRITES = {
    "WL-FILES-07": "LAW — `workshop/filesystem_roots.cpp` is the only place this repository asks an operating system for its roots, and the keyboard-readiness test may not ask for them: its whole test is in memory.",
    "WL-FILES-11": "LAW — `workshop/path_admission.hpp` alone asks for a path's bytes: a path is admitted as a value (carried or not, plus its spelling), a filename as a name with an exact flag, the launch capture as the host's.",
}

# ONE SPELLING CORRECTED BEFORE IT IS RE-PATHED. WL-DOC-10 ("creating mints, deleting removes
# exactly one identity") is witnessed by the free function `create` in screen.hpp, whose
# pointer says so; the rule-s sweep respelled it `LayoutTabPress::create` -- a bool data
# member of the tab-press shape, the wrong `create` -- and rule n let it pass because a
# qualified spelling matches a declaration by its last part. With the free function's body
# in screen_gestures.cpp the suffix no longer covers it, so the register says what the
# pointer always said: (register, header, spelling as written) -> spelling meant.
SPELLING_CORRECTIONS = {
    ("agents/workshop/document.md", "workshop/screen.hpp", "LayoutTabPress::create"): "create",
}

WRAP = 98


def git_show(repo, commit, path):
    return subprocess.run(["git", "-C", repo, "show", "%s:%s" % (commit, path)], check=True,
                          capture_output=True).stdout.decode("utf-8")


def dest_path(header, subject):
    stem = os.path.basename(header)[:-4]
    if header in ("workshop/files.hpp", "workshop/filesystem_roots.hpp"):
        return "workshop/%s.cpp" % subject
    return "workshop/%s_%s.cpp" % (stem, subject)


# ---- reading START ------------------------------------------------------------------------

class Header:
    def __init__(self, repo, commit, path):
        self.path = path
        self.text = git_show(repo, commit, path)
        self.lines = self.text.split("\n")
        self.decls, self.masked = cxx.declarations(self.text)
        self.code = [n for n, _ in cxx.code_lines(self.text)]
        self.indent = INDENT_OF.get(path, "")
        self.banners = [i + 1 for i, l in enumerate(self.lines) if l.startswith(self.indent + "// ----")]
        cls = CLASS_OF.get(path)
        self.nested_types = set()
        if cls:
            self.nested_types = {d.name for d in self.decls if d.kind in ("type", "alias") and d.scope and d.scope[-1] == cls}
        self.movable = []
        for d in self.decls:
            if d.kind != "function" or not d.defined:
                continue
            if cls:
                if not d.scope or d.scope[-1] != cls:
                    continue
            else:
                if any(s and s[0].isupper() for s in d.scope):
                    continue  # a struct's own member, defined in-class: stays
            if "constexpr" in d.specifiers or d.template:
                continue
            if path in ONLY and d.name not in ONLY[path]:
                continue
            self.movable.append(d)
        import bisect
        for d in self.movable:
            k = bisect.bisect_left(self.code, d.first_line)
            prev = self.code[k - 1] if k > 0 else 0
            d.pointer_lines = cxx.pointer_lines_between(self.lines, prev, d.first_line)
            d.pointer_ids = re.findall(r"WL-[A-Z]+-[0-9]+", "\n".join(self.lines[i - 1] for i in d.pointer_lines))
            d.section_line = max([b for b in self.banners if b < d.first_line] or [1])
            d.section = self.lines[d.section_line - 1].strip() if d.section_line in self.banners else ""
        self.blocks = []
        if path in ("workshop/files.hpp", "workshop/filesystem_roots.hpp"):
            self.blocks = [self.win32_block()]

    def win32_block(self):
        """The file-scope `#if defined(_WIN32) ... #include <windows.h> ... #endif` block,
        with the comment lines directly above it: (first_line, last_line)."""
        for i, l in enumerate(self.lines):
            if l.startswith("#if defined(_WIN32)"):
                j = i
                depth = 0
                while True:
                    x = self.lines[j]
                    if x.startswith(("#if", "#ifdef", "#ifndef")):
                        depth += 1
                    elif x.startswith("#endif"):
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                if any("<windows.h>" in x for x in self.lines[i:j + 1]):
                    k = i
                    while k > 0 and self.lines[k - 1].startswith("//"):
                        k -= 1
                    return (k + 1, j + 1)
        raise SystemExit("no <windows.h> block in %s" % self.path)

    def subject_of(self, d):
        subject = None
        for line, name, _ in PLAN[self.path]:
            if line <= d.section_line:
                subject = name
        return subject


def spelling_of(d, header):
    """How PROVEN BY would spell this row: `detail::name` for a nested namespace's
    function, the bare name otherwise (WorkshopWeave's members are bare in the registers)."""
    cls = CLASS_OF.get(header.path)
    scope = [s for s in d.scope if s != "zengine::workshop"]
    if cls and scope and scope[-1] == cls:
        return d.name
    if scope:
        return "::".join(scope + [d.name])
    return d.name


# ---- the sheet ----------------------------------------------------------------------------

SHEET_COLUMNS = ["file", "section_line", "section", "scope", "name", "spelling", "first_param",
                 "kind", "destination", "pointer_ids", "start_line", "end_line", "rewrites"]


def seed(repo, commit, out):
    rows = []
    for path in HEADERS:
        h = Header(repo, commit, path)
        for d in h.movable:
            rewrites = ["inline"] if "inline" in d.specifiers else []
            for s in ("static", "explicit", "virtual"):
                if s in d.specifiers:
                    rewrites.append(s)
            if re.search(r"(?<![=!<>])=(?!=)", d.params_text):
                rewrites.append("default-argument")
            cls = CLASS_OF.get(path)
            if cls:
                rewrites.append("WorkshopWeave::")
                ret = re.sub(r"\[\[[^\]]*\]\]", "", h.text[d.start:d.paren_open])
                words = cxx.IDENT.findall(cxx.strip_angles(ret))[:-1]
                if any(w in h.nested_types for w in words):
                    rewrites.append("nested-return")
            rows.append({
                "file": path, "section_line": d.section_line, "section": d.section,
                "scope": "::".join(d.scope), "name": d.name, "spelling": spelling_of(d, h),
                "first_param": d.first_param, "kind": "function",
                "destination": dest_path(path, h.subject_of(d)),
                "pointer_ids": ",".join(d.pointer_ids), "start_line": d.first_line,
                "end_line": d.last_line, "rewrites": ",".join(rewrites)})
        for (a, b) in h.blocks:
            rows.append({"file": path, "section_line": 1, "section": "", "scope": "", "name": "<windows.h>",
                         "spelling": "", "first_param": "", "kind": "block",
                         "destination": dest_path(path, PLAN[path][0][1]), "pointer_ids": "",
                         "start_line": a, "end_line": b, "rewrites": "preprocessor-block"})
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The Workshop split's sheet, regenerated from %s by apply.py seed; one row per moved body.\n" % commit)
        f.write("\t".join(SHEET_COLUMNS) + "\n")
        for r in rows:
            f.write("\t".join(str(r[c]) for c in SHEET_COLUMNS) + "\n")
    return rows


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
        r["section_line"] = int(r["section_line"])
    return rows


# ---- the rewrite of one body --------------------------------------------------------------

def dedent(lines, indent):
    out = []
    for l in lines:
        out.append(l[len(indent):] if indent and l.startswith(indent) else l)
    return out


def strip_default_arguments(sig, masked_sig, paren_open, paren_close):
    """Remove ` = <expr>` at parameter depth from sig[paren_open+1:paren_close]."""
    cuts = []
    depth = 0
    i = paren_open + 1
    while i < paren_close:
        c = masked_sig[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "=" and depth == 0 and masked_sig[i + 1] != "=" and masked_sig[i - 1] not in "=!<>":
            j = i + 1
            d2 = 0
            while j < paren_close:
                cj = masked_sig[j]
                if cj in "([{":
                    d2 += 1
                elif cj in ")]}":
                    if d2 == 0:
                        break
                    d2 -= 1
                elif cj == "," and d2 == 0:
                    break
                j += 1
            k = i
            while k > paren_open and sig[k - 1] in " \t":
                k -= 1
            cuts.append((k, j))
            i = j
            continue
        i += 1
    for a, b in reversed(cuts):
        sig = sig[:a] + sig[b:]
    return sig


def definition_of(h, d, row):
    """The out-of-line definition text of one moved function, as a list of lines."""
    cls = CLASS_OF.get(h.path)
    text = h.text
    line_start = text.rfind("\n", 0, d.start) + 1
    sig = text[line_start:d.body_open]
    body = text[d.body_open:d.body_close + 1]
    # positions inside sig
    po = d.paren_open - line_start
    pc = d.paren_close - line_start
    msig = h.masked[line_start:d.body_open]
    old_paren_col = po - (sig.rfind("\n", 0, po) + 1)
    # 1. declaration-only specifiers and `inline`, in the head before the name
    head = sig[:po]
    rest = sig[po:]
    m = re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*$", head)
    if not m:
        raise SystemExit("no name before ( in %s:%d" % (h.path, d.first_line))
    name_at = m.start()
    pre = head[:name_at]
    for word in ("inline", "static", "explicit", "virtual"):
        pre = re.sub(r"(?<![A-Za-z0-9_])%s[ \t]+" % word, "", pre)
    # 2. nested return type
    if cls:
        for t in sorted(h.nested_types, key=len, reverse=True):
            pre = re.sub(r"(?<![A-Za-z0-9_:])%s(?![A-Za-z0-9_])" % re.escape(t), "%s::%s" % (cls, t), pre)
    # 3. qualification
    qual = "%s::" % cls if cls else ""
    new_head = pre + qual + head[name_at:]
    new_sig = new_head + rest
    # 4. default arguments (positions shifted by the head's change)
    delta = len(new_head) - len(head)
    new_msig = cxx.mask(new_sig)[0]
    new_sig = strip_default_arguments(new_sig, new_msig, po + delta, pc + delta)
    lines = (new_sig + body).split("\n")
    # continuation lines of the signature aligned to the old `(` column follow the new one
    new_paren_col = (po + delta) - (new_sig.rfind("\n", 0, po + delta) + 1)
    sig_line_count = new_sig.count("\n") + 1
    indent = h.indent
    out = []
    for i, l in enumerate(lines):
        if 0 < i < sig_line_count:
            lead = len(l) - len(l.lstrip(" "))
            if lead == old_paren_col + 1 and l.strip():
                out.append(" " * (new_paren_col + 1) + l.lstrip(" "))
                continue
        out.append(l)
    return dedent(out, indent)


def prototype_of(h, d):
    text = h.text
    proto = cxx.prototype_text(text, h.masked, d)
    after = text[d.body_close + 1:text.find("\n", d.body_close)]
    if after.strip():
        if after.strip().startswith("//"):
            proto += " " + after.strip()
        else:
            raise SystemExit("code after the body's } on its line at %s:%d" % (h.path, d.last_line))
    return proto.split("\n")


def workshop_law_line(pointer_lines):
    regs = collections.Counter()
    for l in pointer_lines:
        for r in re.findall(r"agents/workshop/[a-z-]+\.md", l):
            regs[r] += 1
    if not regs:
        return None
    first = sorted(regs.items(), key=lambda kv: (-kv[1], kv[0]))[0][0]
    n = len(regs) - 1
    if n == 0:
        return "// Workshop law: %s" % first
    return "// Workshop law: %s (+%d register%s; agents/workshop.md routes)" % (first, n, "" if n == 1 else "s")


# ---- apply --------------------------------------------------------------------------------

def apply(repo, commit, sheet_path):
    rows = read_sheet(sheet_path)
    headers = {p: Header(repo, commit, p) for p in HEADERS}
    by_file = collections.defaultdict(list)
    for r in rows:
        by_file[r["file"]].append(r)
    cpp_items = collections.defaultdict(list)  # destination -> list of (header, kind, payload)
    written = []
    for path in HEADERS:
        h = headers[path]
        decl_at = {(d.first_line, d.last_line): d for d in h.movable}
        edits = []  # (first_line, last_line, replacement_lines)
        drop = set()
        for r in by_file[path]:
            if r["kind"] == "block":
                edits.append((r["start_line"], r["end_line"], []))
                cpp_items[r["destination"]].append((h, "block", (r["start_line"], r["end_line"])))
                continue
            d = decl_at.get((r["start_line"], r["end_line"]))
            if d is None or d.name != r["name"]:
                raise SystemExit("sheet row %s:%s-%s %s does not match START" % (path, r["start_line"], r["end_line"], r["name"]))
            edits.append((d.first_line, d.last_line, prototype_of(h, d)))
            drop.update(d.pointer_lines)
            cpp_items[r["destination"]].append((h, "function", d))
        # pointer lines are dropped by their ORIGINAL numbers: mark them before editing.
        marked = list(h.lines)
        for n in drop:
            marked[n - 1] = "\x00DROP"
        new_lines = marked
        for a, b, repl in sorted(edits, reverse=True):
            new_lines[a - 1:b] = repl
        new_lines = [l for l in new_lines if l != "\x00DROP"]
        new_text = "\n".join(new_lines)
        law = workshop_law_line([l for l in new_lines if cxx.POINTER_LINE.match(l)])
        if law:
            new_text = WORKSHOP_LAW.sub(law, new_text, count=1)
        write(repo, path, new_text)
        written.append(path)
    # the .cpp files
    plan_desc = {dest_path(p, s): (p, s, desc) for p in PLAN for (_, s, desc) in PLAN[p]}
    sources = []
    for dest in sorted(cpp_items):
        header_path, subject, desc = plan_desc[dest]
        h = headers[header_path]
        cls = CLASS_OF.get(header_path)
        items = cpp_items[dest]
        out = [SPDX.rstrip("\n"), ""]
        pointer_lines = []
        body_lines = []
        blocks = [it for it in items if it[1] == "block"]
        funcs = [it for it in items if it[1] == "function"]
        funcs.sort(key=lambda it: it[2].first_line)
        last_section = None
        open_ns = []
        for hh, _, d in funcs:
            scope = [s for s in d.scope if s not in ("zengine::workshop",) and s != cls]
            if scope != open_ns:
                for s in reversed(open_ns):
                    body_lines += ["", "} // namespace %s" % s]
            if d.section_line != last_section and d.section:
                body_lines += ["", d.section]
            last_section = d.section_line
            if scope != open_ns:
                open_ns = scope
                for s in scope:
                    body_lines += ["", "namespace %s {" % s]
            ptrs = [hh.lines[i - 1].strip() for i in d.pointer_lines]
            pointer_lines += ptrs
            body_lines += [""] + ptrs + definition_of(hh, d, None)
        for s in reversed(open_ns):
            body_lines += ["", "} // namespace %s" % s]
        law = workshop_law_line(pointer_lines)
        import textwrap
        sentence = ("The bodies of `%s`'s %s -- %s -- compiled once into `zengine-workshop-logic` and linked "
                    "by the host and every suite; the declarations, the constants and the constexpr "
                    "functions stay in the header." % (os.path.basename(header_path),
                                                       "section" if header_path in ONLY else "sections", desc))
        out += ["// " + l for l in textwrap.wrap(sentence, width=WRAP - 3, break_on_hyphens=False)]
        if law:
            out.append(law)
        out.append("")
        for hh, _, (a, b) in blocks:
            out += hh.lines[a - 1:b] + [""]
        out.append('#include "%s"' % os.path.basename(header_path))
        out.append("")
        out.append("namespace zengine::workshop {")
        out += body_lines
        out += ["", "} // namespace zengine::workshop", ""]
        write(repo, dest, "\n".join(out))
        written.append(dest)
        sources.append(dest)
    # the registers
    mapping = repath_registers(repo, commit, headers, rows)
    for reg, text in mapping.items():
        write(repo, reg, text)
        written.append(reg)
    # the CMake source list, between its markers, read from the working tree
    cm = os.path.join(repo, "workshop/CMakeLists.txt")
    with open(cm, encoding="utf-8") as f:
        cmt = f.read()
    a = cmt.index("add_library(zengine-workshop-logic STATIC\n")
    b = cmt.index("\n)", a)
    listing = "\n".join("    %s" % os.path.basename(s) for s in sources)
    cmt = cmt[:a] + "add_library(zengine-workshop-logic STATIC\n" + listing + cmt[b:]
    write(repo, "workshop/CMakeLists.txt", cmt)
    written.append("workshop/CMakeLists.txt")
    print("apply: wrote %d files" % len(written))
    for w in written:
        print("  ", w)


def write(repo, rel, text):
    with open(os.path.join(repo, rel), "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


# ---- the registers ------------------------------------------------------------------------

PATH_TOKEN = re.compile(r"`(workshop/(?:screen|weave|files|filesystem_roots)\.hpp)`")


def paragraphs(text):
    """(start_line_index, end_line_index_exclusive) of every PROVEN BY paragraph."""
    lines = text.split("\n")
    out = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("PROVEN BY"):
            j = i + 1
            while j < len(lines) and lines[j].strip() and not lines[j].startswith(("WHY", "UNWITNESSED")):
                j += 1
            out.append((i, j))
            i = j
        else:
            i += 1
    return out, lines


def split_segments(joined, header):
    """Find `header` segments in a joined paragraph: (start, end, [ids], trailer)."""
    out = []
    for m in PATH_TOKEN.finditer(joined):
        if m.group(1) != header:
            continue
        i = m.end()
        ids = []
        while True:
            mm = re.match(r"\s*`([^`]+)`", joined[i:])
            if not mm or mm.group(1).startswith('"') or mm.group(1).startswith("case"):
                break
            ids.append(mm.group(1))
            i += mm.end()
            sep = re.match(r"\s*,", joined[i:])
            if sep:
                i += sep.end()
                continue
            break
        out.append((m.start(), i, ids))
    return out


def wrap_from_change(old_lines, new_joined, width=WRAP):
    """Keep every leading line of the old paragraph the change did not reach; greedy-wrap
    the rest at `width` bytes."""
    kept = []
    rest = new_joined
    for l in old_lines:
        if rest.startswith(l) and (len(rest) == len(l) or rest[len(l)] == " "):
            kept.append(l)
            rest = rest[len(l):].lstrip(" ")
        else:
            break
    words = rest.split(" ") if rest else []
    cur = ""
    for w in words:
        if not cur:
            cur = w
        elif len((cur + " " + w).encode("utf-8")) <= width:
            cur += " " + w
        else:
            kept.append(cur)
            cur = w
    if cur:
        kept.append(cur)
    return kept


def build_index(headers, rows):
    """For each header: the moved rows and the START declarations that stay."""
    idx = {}
    for path, h in headers.items():
        moved_keys = {(r["start_line"], r["end_line"]) for r in rows if r["file"] == path and r["kind"] == "function"}
        dest_of = {(r["start_line"], r["end_line"]): r["destination"] for r in rows if r["file"] == path and r["kind"] == "function"}
        moved = [(d, dest_of[(d.first_line, d.last_line)]) for d in h.movable if (d.first_line, d.last_line) in moved_keys]
        moved_ids = {id(d) for d, _ in moved}
        staying = [d for d in h.decls if id(d) not in moved_ids and d.kind != "namespace"]
        idx[path] = (moved, staying)
    return idx


def matches(spelling, d, cls):
    m = re.match(r"^([A-Za-z_][A-Za-z0-9_:]*)\(([A-Za-z_][A-Za-z0-9_:]*)\)$", spelling)
    if m:
        name, param = m.group(1), m.group(2)
        if d.kind != "function":
            return False
        if "::" in name:
            return matches(name, d, cls) and getattr(d, "first_param", "") == param.split("::")[-1]
        return d.name == name and getattr(d, "first_param", "") == param.split("::")[-1]
    parts = spelling.split("::")
    if len(parts) == 1:
        return d.name == spelling
    scope = [s.split("::")[-1] for s in d.scope]
    if d.name != parts[-1]:
        return False
    want = parts[:-1]
    return scope[-len(want):] == want


def namespace_scoped(d, cls):
    return not any(s and s[0].isupper() for s in d.scope)


def destinations_for(spelling, path, idx, end_code):
    """The files a PROVEN BY spelling under `path` belongs under at END, in order."""
    cls = CLASS_OF.get(path)
    moved, staying = idx[path]
    hits_moved = [dest for d, dest in moved if matches(spelling, d, cls)]
    stays = [d for d in staying if matches(spelling, d, cls)]
    if "::" not in spelling and "(" not in spelling:
        # a bare name is a namespace-scope thing (rule s spells a member Struct::member);
        # a struct's own member of the same name does not keep the header
        ns_stays = [d for d in stays if namespace_scoped(d, cls) or (cls and d.scope and d.scope[-1] == cls and d.kind != "function")]
        if hits_moved or ns_stays:
            stays = ns_stays
    out = []
    if stays:
        out.append(path)
    for dest in hits_moved:
        if dest not in out:
            out.append(dest)
    if out:
        return out, "declared"
    # used, not declared: follow the token
    parts = cxx.IDENT.findall(spelling)
    def has(code):
        return all(re.search(r"(^|[^A-Za-z0-9_])%s([^A-Za-z0-9_]|$)" % re.escape(p), code) for p in parts)
    if has(end_code[path]):
        return [path], "used-in-header"
    # A name the register cites as a USE rather than a declaration (`GetLogicalDrives`,
    # `admit_location` under weave.hpp) follows the body that uses it, and is listed once,
    # under the first destination in START order that holds it: one spelling in, one out.
    files = [f for f in end_code if f != path and (f.startswith(path[:-4] + "_") or f == path[:-4] + ".cpp")]
    hit = [f for f in files if has(end_code[f])]
    if hit:
        return hit[:1], "used-in-body"
    return [path], "unresolved"


def repath_registers(repo, commit, headers, rows, report=None):
    idx = build_index(headers, rows)
    # END code of every touched file, comments stripped, for the used-not-declared fallback
    end_code = {}
    for path in HEADERS:
        with open(os.path.join(repo, path), encoding="utf-8") as f:
            end_code[path] = cxx.strip_comments_and_blanks(f.read())
    # destinations in START order of their first row, so "the first body" is well defined
    seen = []
    for r in sorted(rows, key=lambda r: (HEADERS.index(r["file"]), r["start_line"])):
        if r["destination"] not in seen:
            seen.append(r["destination"])
    for dest in seen:
        with open(os.path.join(repo, dest), encoding="utf-8") as f:
            end_code[dest] = cxx.strip_comments_and_blanks(f.read())
    out = {}
    regs = subprocess.run(["git", "-C", repo, "ls-tree", "--name-only", commit, "agents/workshop/"],
                          check=True, capture_output=True).stdout.decode().split()
    decisions = []
    for reg in sorted(regs):
        if not reg.endswith(".md"):
            continue
        text = git_show(repo, commit, reg)
        paras, lines = paragraphs(text)
        new_lines = list(lines)
        for (a, b) in reversed(paras):
            old = lines[a:b]
            joined = " ".join(l.strip() for l in old)
            changed = False
            for header in HEADERS:
                segs = split_segments(joined, header)
                for (s, e, ids) in reversed(segs):
                    groups = collections.OrderedDict()
                    for sp in ids:
                        sp = SPELLING_CORRECTIONS.get((reg, header, sp), sp)
                        dests, how = destinations_for(sp, header, idx, end_code)
                        decisions.append((reg, header, sp, dests, how))
                        for dst in dests:
                            groups.setdefault(dst, []).append(sp)
                    pieces = []
                    for dst, sps in groups.items():
                        pieces.append("`%s` %s" % (dst, ", ".join("`%s`" % x for x in sps)))
                    replacement = "; ".join(pieces)
                    if replacement != joined[s:e]:
                        joined = joined[:s] + replacement + joined[e:]
                        changed = True
            if changed:
                new_lines[a:b] = wrap_from_change(old, joined)
        # the two LAW lines
        for i, l in enumerate(new_lines):
            if l.startswith("## "):
                m = re.match(r"^## (WL-[A-Z]+-[0-9]+) ", l)
                if m and m.group(1) in LAW_REWRITES:
                    j = i + 1
                    while not new_lines[j].startswith("LAW"):
                        j += 1
                    new_lines[j] = LAW_REWRITES[m.group(1)]
        new_text = "\n".join(new_lines)
        if new_text != text:
            out[reg] = new_text
        if report is not None:
            report[reg] = (len(text.encode("utf-8")), len(new_text.encode("utf-8")))
    if report is not None:
        report["__decisions__"] = decisions
    return out


def budget(repo, commit, sheet_path):
    """Compute every register's byte count after re-pathing, without writing anything: the
    END headers and .cpp files are generated in memory into a scratch copy."""
    import tempfile, shutil
    rows = read_sheet(sheet_path)
    tmp = tempfile.mkdtemp(prefix="workshop-split-budget-")
    try:
        for sub in ("workshop", "agents/workshop"):
            os.makedirs(os.path.join(tmp, sub), exist_ok=True)
        # a scratch repo: only the files apply touches, from START
        with open(os.path.join(tmp, "start.tar"), "wb") as tar:
            subprocess.run(["git", "-C", repo, "archive", commit, "workshop", "agents/workshop"], check=True,
                           stdout=tar)
        subprocess.run(["tar", "-xf", "start.tar"], cwd=tmp, check=True)
        subprocess.run(["git", "-C", tmp, "init", "-q"], check=True)
        subprocess.run(["git", "-C", tmp, "add", "-A"], check=True)
        subprocess.run(["git", "-C", tmp, "-c", "user.name=x", "-c", "user.email=x@x", "commit", "-q", "-m", "start"], check=True)
        start = subprocess.run(["git", "-C", tmp, "rev-parse", "HEAD"], check=True, capture_output=True).stdout.decode().strip()
        cm = os.path.join(tmp, "workshop/CMakeLists.txt")
        with open(cm, "a", encoding="utf-8") as f:
            f.write("\nadd_library(zengine-workshop-logic STATIC\n)\n")
        apply(tmp, start, sheet_path)
        headers = {p: Header(tmp, start, p) for p in HEADERS}
        report = {}
        repath_registers(tmp, start, headers, rows, report)
        over = 0
        print("register\tSTART bytes\tEND bytes\tdelta\tleft of 16384")
        for reg in sorted(k for k in report if not k.startswith("__")):
            a, b = report[reg]
            flag = "  OVER" if b > 16384 else ("  <300 left" if 16384 - b < 300 else "")
            if b > 16384:
                over += 1
            print("%s\t%d\t%d\t%+d\t%d%s" % (reg, a, b, b - a, 16384 - b, flag))
        how = collections.Counter(d[4] for d in report["__decisions__"])
        print("decisions:", dict(how))
        for d in report["__decisions__"]:
            if d[4] in ("used-in-body", "unresolved") or len(d[3]) > 1:
                print("  ", d)
        print("registers over budget:", over)
        return over
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["seed", "apply", "budget"])
    ap.add_argument("--repo", required=True)
    ap.add_argument("--start", required=True)
    ap.add_argument("--sheet", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "sheet.tsv"))
    a = ap.parse_args()
    if a.mode == "seed":
        rows = seed(a.repo, a.start, a.sheet)
        print("seed: %d rows -> %s" % (len(rows), a.sheet))
    elif a.mode == "apply":
        apply(a.repo, a.start, a.sheet)
    else:
        sys.exit(1 if budget(a.repo, a.start, a.sheet) else 0)


if __name__ == "__main__":
    main()
