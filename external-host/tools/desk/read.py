# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""desk/read -- the desk said whole: a running Workshop's desk read under the guest's own row and
written for a model as text files, into the one directory `out` names.

In order: `out` is judged (never inside a source checkout, never a file, never an AGENTS.md this
reader did not write); `AGENTS.md` is written first, the reader's fixed words and nothing the
desk said; then the asks (desk_asks): the asker's own row from the guest door, the desk in one
turn with every pane past it paged, the pane inventory and the keymap; then the readings
(desk_render) beside it: `guide.md`, `desk.txt`, `glance.txt` and `panes/<office>.<pane>.txt`,
each overwritten, each paged past 64 KiB. A reading file this read did not write again -- a pane
gone from the desk, a page a shorter reading no longer needs -- is removed, so the directory holds
the last reading only.

The files are written with ordinary file I/O: the run context's `produce` takes plain names in
the run's own directory, and a reading lives in a directory of its own with a `panes/` beneath it.
Nothing is written anywhere but `out`. The result names `out` and AGENTS.md's path, so AGENTS.md
is the agent's first read. A refusal of any ask is written into the reading in words; the run
fails only when `out` is refused, and then nothing is written and nothing is asked.
"""

import os
import re
import time

import desk_asks
import desk_render as render
import desk_words as words

#: Pages of a reading a shorter one leaves behind, removed when not written again.
OLD_PAGES = re.compile(r"^(desk|glance)-\d+\.txt$|^guide-\d+\.md$")
#: ...and the whole desk's files, which a watch of some panes does not keep, removed by it.
DESK_FILES = re.compile(r"^(desk|glance)(-\d+)?\.txt$")


def checkout_of(path):
    """The nearest directory from `path` up to its root that holds `.git` -- a checkout's root,
    or a worktree's, whose `.git` is a file -- or None."""
    here = path
    while True:
        if os.path.lexists(os.path.join(here, ".git")):
            return here
        parent = os.path.dirname(here)
        if parent == here:
            return None
        here = parent


def output_dir(ctx, given):
    """The directory the readings go in, judged before anything is asked or written."""
    out = os.path.expanduser((given or "").strip())
    ctx.check(out, "refused: `out` names no directory; desk/read writes only into the directory "
                   "`out` names")
    ctx.check(os.path.isabs(out), "refused: `out` is '%s', not an absolute path: name the "
                                  "directory whole, outside every source checkout" % out)
    for where in (os.path.abspath(out), os.path.realpath(out)):
        root = checkout_of(where)
        ctx.check(root is None,
                  "refused: `out` (%s) lies inside the source checkout at %s, which holds .git. "
                  "A reading is never written inside a checkout, where a coding agent would load "
                  "its AGENTS.md as standing instructions: name a directory outside every "
                  "checkout" % (out, root))
    out = os.path.abspath(out)
    ctx.check(not os.path.exists(out) or os.path.isdir(out),
              "refused: `out` (%s) is a file, not a directory" % out)
    panes = os.path.join(out, "panes")
    ctx.check(not os.path.lexists(panes) or (
        os.path.isdir(panes) and not os.path.islink(panes) and os.path.normcase(
            os.path.realpath(panes)) == os.path.normcase(
                os.path.join(os.path.realpath(out), "panes"))),
              "refused: %s is a file or a link; desk/read writes each pane's reading into a "
              "directory of its own there" % panes)
    agents = os.path.join(out, "AGENTS.md")
    if os.path.exists(agents):
        with open(agents, "r", encoding="utf-8", errors="replace") as f:
            first = f.readline().rstrip("\r\n")
        ctx.check(first == words.MARK,
                  "refused: `out` (%s) holds an AGENTS.md desk/read did not write; name a "
                  "directory of the reader's own" % out)
    return out


#: How long a replacement refused while another reader holds the file open is tried again, in
#: seconds: Windows refuses to replace a file open without delete sharing, as most readers open it.
REPLACE_RETRY = 1.0


def write(path, text):
    """`text` as the whole of `path`, UTF-8 with newline line ends, replacing it in one step: a
    reader of the file sees the last reading or this one, never half of either. A replacement
    refused while another reader holds the file is tried again for REPLACE_RETRY seconds; then
    PermissionError."""
    partial = os.path.join(os.path.dirname(path), "." + os.path.basename(path) + ".partial")
    if os.path.lexists(partial):  # never written through: a link there is removed, not followed
        os.remove(partial)
    with open(partial, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    end = time.monotonic() + REPLACE_RETRY
    while True:
        try:
            os.replace(partial, path)
            return
        except PermissionError:
            if time.monotonic() >= end:
                raise
            time.sleep(0.05)


def readings(reading, only=None):
    """Every reading file but AGENTS.md, as `[(path relative to out, text)]`, in the order they
    are written: the guide, the desk, the glance, then each pane. `only`, a set of (office, pane):
    those panes' files and nothing else, as a watch of some panes writes them."""
    panes = pane_files(reading)
    if only is not None:
        return [(rel, text) for ref, rel, text in panes if ref in only]
    desk_text = render.sparse(reading)
    glance_text = render.glance(reading)
    desk_pages = render.pages(desk_text, "desk.txt")
    glance_pages = render.pages(glance_text, "glance.txt", max_lines=render.glance_band(reading))
    sizes = {"desk.txt": (len(desk_text), len(desk_pages)),
             "glance.txt": (len(glance_text), len(glance_pages))}
    files = render.pages(render.guide(reading, sizes), "guide.md") + desk_pages + glance_pages
    return files + [(rel, text) for _, rel, text in panes]


def pane_files(reading):
    """Each pane's file pages, `[((office, pane), path relative to out, text)]`, front to back."""
    files = []
    if reading.get("desk") is not None:
        taken = set()
        for dp, read in render.pane_reads(reading):
            ref = (dp.get("provider", ""), dp.get("pane", ""))
            name = render.pane_file_name(*ref)
            text = render.pane_file(reading, dp, read)
            # EVERY PAGE OPENS WITH ITS PANE'S HEADER, so a page names its pane whatever its file is
            # called.
            head = next((l for l in text.splitlines() if l.startswith("== ")), None)
            stem, n = name[:-len(".txt")], 2
            paged = render.pages(text, name, again=head)
            # A FILE NAME IS ONE PANE'S: two references that read as one name, or one pane's
            # name another's later page already has, are told apart.
            while any(page.lower() in taken for page, _ in paged):
                name, n = "%s~%d.txt" % (stem, n), n + 1
                paged = render.pages(text, name, again=head)
            taken.update(page.lower() for page, _ in paged)
            files += [(ref, "panes/" + page, page_text) for page, page_text in paged]
    return files


def tidy(out, written, desk_files=False):
    """Remove the reading files a last read wrote and this one did not; `desk_files`, the whole
    desk's too."""
    kept = set(os.path.normcase(os.path.join(out, *rel.split("/"))) for rel, _ in written)
    for name in os.listdir(out):
        path = os.path.join(out, name)
        if (OLD_PAGES.match(name) or (desk_files and DESK_FILES.match(name))) and \
                os.path.isfile(path) and not os.path.islink(path) and \
                os.path.normcase(path) not in kept:
            os.remove(path)
    panes = os.path.join(out, "panes")
    for name in os.listdir(panes):
        path = os.path.join(panes, name)
        if name.endswith(".txt") and os.path.isfile(path) and not os.path.islink(path) and \
                os.path.normcase(path) not in kept:
            os.remove(path)


def result(out, agents, reading, written):
    """The run's one line: AGENTS.md's path first, then where the readings are and what they say."""
    head = "Read %s first. desk/read wrote the desk into %s" % (agents, out)
    if reading.get("desk") is None:
        return "%s; the desk was not read (%s), said in desk.txt" % (
            head, render.clean(reading.get("refused")))[:1900]
    panes = reading.get("panes") or {}
    unread = sum(1 for r in panes.values() if r.get("view") is None)
    line = "%s: guide.md, desk.txt, glance.txt and %d pane file(s) in panes/; %d of %d presented " \
           "pane(s) read" % (head, sum(1 for rel, _ in written if rel.startswith("panes/")),
                             len(panes) - unread, len(panes))
    if unread:
        line += ", %d said not read in desk.txt" % unread
    numbers = reading.get("numbers") or []
    if reading.get("moved"):
        line += "; the desk moved while it was read, desk %d to %d, said in desk.txt" % (
            numbers[0], numbers[-1])
    if reading.get("unconfirmed"):
        line += "; the desk number was not confirmed after the last page, said in desk.txt"
    return (line + "; %d ask(s)" % reading.get("asks", 0))[:1900]


def run(ctx):
    ctx.step("out")
    out = output_dir(ctx, ctx.inputs.get("out"))
    link = ctx.inputs.get("link") or "workshop"
    os.makedirs(os.path.join(out, "panes"), exist_ok=True)
    ctx.step("AGENTS.md")
    agents = os.path.join(out, "AGENTS.md")
    write(agents, words.AGENTS_MD)  # first, and before any ask: the reader's own words only
    reading = desk_asks.read_all(ctx, link)
    ctx.step("readings")
    written = readings(reading)
    for rel, text in written:
        write(os.path.join(out, *rel.split("/")), text)
    tidy(out, written)
    return result(out, agents, reading, written)
