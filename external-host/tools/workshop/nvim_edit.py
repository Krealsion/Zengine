# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/nvim-edit -- make structured edits to one file through Workshop's Neovim pane, save it
through Neovim, and check that the file on disk is exactly what the edits describe.
loom-tool.json lists the operations and their arguments.

THE EDITOR DOES THE EDITING. The tool reads the file only to plan -- to find each anchor's line in
the text it expects -- and then presses the keys a maker would press in Neovim: go to a line, open
a line, type, delete a range, write. It never writes the file itself. After Neovim writes, the
tool reads the file back and compares it with the text the edits should produce; a difference
fails the run and keeps both texts. That comparison is a LOCAL OBSERVATION of this machine's file,
made after Neovim itself has said the buffer is saved.

TYPED TEXT STAYS LITERAL. A buffer's indent script can turn 'autoindent' back on after 'paste'
(Python's does), which stacks each line's indentation onto the next. After opening the file the
tool sets 'paste' again and clears autoindent, smartindent, cindent, indentexpr and expandtab for
that buffer. A new file is written with Unix line endings.

NEOVIM READS ITS INPUT LATER. Workshop's settle fence covers Workshop's own bus, not Neovim's input
queue, so a row read right after a key may still describe the moment before it. The tool
therefore never judges Neovim by one such row. It types a lone `:` and waits until Neovim sits
IDLE IN AN EMPTY COMMAND LINE -- the pane's status row says COMMAND and Neovim's last row is
exactly `:`, twice, a moment apart: only that `:` can leave such a line, so everything typed
before it is done. A prompt (a swap file's question, a y/n) ignores the `:` and never gets
there; the run then fails and types nothing more. From that command line it asks NEOVIM ITSELF,
in one answer carrying a fresh token (so an older answer is never read as this one): is the
current buffer exactly this file, is it modified, and how many lines and which text (a digest)
does it hold.

WHAT IT CONFIRMS BEFORE IT TYPES INTO A BUFFER. After `:e`, the edit goes ahead only when the
buffer IS the requested file, is NOT modified, and holds exactly the lines on disk (an absent
file: none). A draft never saved to disk, an unsaved change to an existing file, a buffer that
differs from the disk, and an `:e` Neovim refused -- another buffer still current -- all refuse
with nothing typed into any buffer. An absent file is not evidence of an empty buffer.

WHAT SUCCESS MEANS. After `:w` the same question is asked again: the run passes only when Neovim
says the buffer is the file, is not modified and holds the planned lines, AND the file on disk is
byte for byte the planned text.

WHAT IT REFUSES. A path Ex would have to escape, a CRLF file, a buffer with unsaved changes (the
tool would not know which text is meant), an anchor that matches no line or several. It does not
undo: a failed run says what Neovim was asked to do and leaves the buffer as it is. A retry after
an interrupted run starts from the buffer and the disk as they are then: typing the interrupted
run left unsaved is refused like any other unsaved work."""
import hashlib
import json
import os
import re
import secrets
import time
from pathlib import Path

from act import painted, wait_rows
from hand import Hand
from workshop_steps import chord_moments, moment

PANE = ("zengine.editor", "editor")
KINDS = ("create", "rewrite", "append", "after", "before", "replace", "delete")
NT = os.name == "nt"


def anchor_at(ctx, lines, anchor, start=0, unique=True):
    hits = [n for n in range(start, len(lines)) if anchor in lines[n]]
    ctx.check(hits, "no line holds %r" % anchor)
    ctx.check(not unique or len(hits) == 1, "%d lines hold %r (lines %s); name a unique anchor"
              % (len(hits), anchor, ", ".join(str(n + 1) for n in hits[:8])))
    return hits[0]


def text_of(ctx, text):
    ctx.check(isinstance(text, str) and text != "", "an edit that types names non-empty text")
    return (text[:-1] if text.endswith("\n") else text).split("\n")


def span(ctx, lines, value):
    """[first, last] anchors (last searched from first on), or one anchor for one line."""
    first, last = (value, value) if isinstance(value, str) else value
    a = anchor_at(ctx, lines, first)
    return a, anchor_at(ctx, lines, last, a, unique=False)


def insert_at(lines, k, new):
    """Keys that put `new` before line index k of `lines` (k == len appends)."""
    if not lines:
        return [("keys", "ggdGi"), ("insert", new), ("press", "escape")]
    if k == 0:
        return [("keys", ":1\nO"), ("insert", new), ("press", "escape")]
    return [("keys", ":%d\no" % k), ("insert", new), ("press", "escape")]


def plan(ctx, edits, lines):
    """Apply every edit to the expected text and say the keys each takes. Nothing is sent."""
    steps, spans = [], []
    for i, edit in enumerate(edits):
        kinds = [k for k in KINDS if isinstance(edit, dict) and k in edit]
        ctx.check(len(kinds) == 1, "edit %d names %s; each names exactly one of %s"
                  % (i, kinds or "nothing", ", ".join(KINDS)))
        kind = kinds[0]
        if kind == "rewrite":
            # The whole file, replaced: the buffer is cleared and the new text typed.
            new = text_of(ctx, edit["rewrite"])
            steps += insert_at([], 0, new)
            lines, where = new, (1, len(new))
            spans.append({"edit": i, "kind": kind, "lines": where})
            continue
        if kind == "create":
            ctx.check(not lines, "create needs an absent or empty file")
            new = text_of(ctx, edit["create"])
            steps += insert_at(lines, 0, new)
            lines, where = new, (1, len(new))
        elif kind == "append":
            new = text_of(ctx, edit["append"])
            steps += insert_at(lines, len(lines), new)
            where = (len(lines) + 1, len(lines) + len(new))
            lines = lines + new
        elif kind in ("after", "before"):
            n = anchor_at(ctx, lines, edit[kind]) + (1 if kind == "after" else 0)
            new = text_of(ctx, edit.get("text"))
            steps += insert_at(lines, n, new)
            lines, where = lines[:n] + new + lines[n:], (n + 1, n + len(new))
        else:
            a, b = span(ctx, lines, edit[kind])
            steps.append(("keys", ":%d,%dd\n" % (a + 1, b + 1)))
            lines = lines[:a] + lines[b + 1:]
            where = (a + 1, b + 1)
            if kind == "replace":
                new = text_of(ctx, edit.get("text"))
                steps += insert_at(lines, a, new)
                lines, where = lines[:a] + new + lines[a:], (a + 1, a + len(new))
        spans.append({"edit": i, "kind": kind, "lines": where})
    return steps, spans, lines


def pane_rows(hand):
    """The Neovim pane's rows now: Workshop's status row first, then Neovim's own screen, its
    command line last. Empty while Workshop does not describe the pane."""
    view = painted(hand, *PANE)
    return [r["text"] for r in view["rows"]] if view and view["rows"] else []


def status(hand):
    """Neovim's pane's top row -- Workshop's account of it: saved or UNSAVED, mode, profile, file."""
    rows = pane_rows(hand)
    return rows[0] if rows else ""


def mode_of(row):
    """The mode word of a status row (`saved NORMAL clean -- ...`), or "" for any other row."""
    words = row.split(" ")
    return words[1] if len(words) > 1 and words[0] in ("saved", "UNSAVED") else ""


def digest_of(lines):
    """How Neovim's answer names a text: the first 16 hex digits of the SHA-256 of its lines
    joined by LF with a final LF. An empty buffer is one empty line to Neovim, so no lines and
    one empty line name the same."""
    return hashlib.sha256(("\n".join(lines or [""]) + "\n").encode("utf-8")).hexdigest()[:16]


def ask(ctx, hand, posix, when):
    """THE FENCE, THEN NEOVIM'S OWN ANSWER. A lone `:`; then wait until Neovim is idle in an
    empty command line (read twice, 0.2 s apart, so a line Neovim is only passing through is not
    taken for it); then ask, and read the answer that carries this ask's token. Returns
    {same, modified, lines, digest, row}: whether the current buffer is `posix`, whether it is
    modified, its line count, its digest (`digest_of`), and Workshop's status row at the fence."""
    hand.inject([moment(ctx, "TextEntered", text=":")])
    end, steady = time.monotonic() + 20, 0
    while steady < 2:
        rows = pane_rows(hand)
        idle = bool(rows) and mode_of(rows[0]) == "COMMAND" and rows[-1] == ":"
        steady = steady + 1 if idle else 0
        if mode_of(rows[0] if rows else "") == "PROMPT":
            ctx.produce("%s-rows.json" % when.replace(" ", "-").replace(":", ""),
                        json.dumps(rows, indent=1).encode())
            ctx.fail("Neovim is waiting at a prompt %s (a swap file's question, or a message it "
                     "wants read): answer it in Neovim; nothing more was typed (its last row "
                     "reads %r)" % (when, rows[-1] if rows else ""))
        if not idle and time.monotonic() >= end:
            ctx.produce("%s-rows.json" % when.replace(" ", "-").replace(":", ""),
                        json.dumps(rows, indent=1).encode())
            ctx.fail("Neovim did not come to an empty command line %s within 20s (its status "
                     "row reads %r, its last row %r); nothing more was typed"
                     % (when, rows[0] if rows else "", rows[-1] if rows else ""))
        time.sleep(0.2 if idle else 0.1)
    token = secrets.token_hex(3)
    # One `let` gathers the facts (it says nothing, so a long line that wraps asks for no
    # Return); a short `echo` says them. Windows paths compare as Windows compares them.
    here = ("tr(expand('%:p'),'\\','/')==?'" if NT else "expand('%:p')==#'") + posix + "'"
    facts = "[%s,&mod,line('$'),sha256(join(getline(1,'$'),\"\\n\").\"\\n\")[0:15]]" % here
    hand.inject([moment(ctx, "TextEntered", text="let g:nvim_edit=%s\n:echo 'nvim-edit' '%s' "
                        "join(g:nvim_edit)\n" % (facts, token))])
    said = re.compile(r"^nvim-edit %s ([01]) ([01]) (\d+) ([0-9a-f]{16})$" % token)
    end = time.monotonic() + 10
    while True:
        rows = pane_rows(hand)
        found = [said.match(r) for r in rows if said.match(r)]
        if found:
            m = found[-1]
            return {"same": m.group(1) == "1", "modified": m.group(2) == "1",
                    "lines": int(m.group(3)), "digest": m.group(4), "row": rows[0]}
        if time.monotonic() >= end:
            ctx.produce("%s-rows.json" % when.replace(" ", "-").replace(":", ""),
                        json.dumps(rows, indent=1).encode())
            ctx.fail("Neovim did not answer what it holds %s (its last row reads %r)"
                     % (when, rows[-1] if rows else ""))
        time.sleep(0.1)


def run(ctx):
    given = Path(ctx.inputs["path"])
    ctx.check(given.is_absolute(), "path must be absolute: Neovim's working directory is Workshop's")
    posix = Path(os.path.normpath(str(given))).as_posix()
    ctx.check(not any(c in posix for c in " %#|\"'\\"), "the path holds a character Ex would "
              "read as something else; move the file or open it by hand")
    path = Path(posix)
    edits = json.loads(ctx.inputs["edits"])
    ctx.check(isinstance(edits, list) and edits, "edits must be a non-empty JSON list")
    chunk = max(200, int(ctx.inputs.get("chunk", 1500)))
    pause = max(0, int(ctx.inputs.get("pace_ms", 0))) / 1000.0
    existed = path.is_file()
    before = path.read_bytes() if existed else b""
    to_unix = bool(ctx.inputs.get("to_unix", False))
    ctx.check(b"\r\n" not in before or to_unix, "the file has CRLF line endings; pass to_unix=true "
              "to have Neovim write it back with LF")
    lines = before.decode("utf-8").replace("\r\n", "\n").split("\n") if before else []
    if lines and lines[-1] == "":
        lines.pop()
    steps, spans, final = plan(ctx, edits, lines)
    expected = ("\n".join(final) + "\n").encode("utf-8") if final else b""
    record = {"path": posix, "existed": existed, "before_sha256": hashlib.sha256(before).hexdigest(),
              "expected_sha256": hashlib.sha256(expected).hexdigest(), "edits": spans,
              "disk_digest": digest_of(lines), "planned_digest": digest_of(final)}
    started = time.monotonic()
    hand = Hand(ctx, ctx.inputs["link"])

    def refuse(verdict, words):
        record.update(verdict=verdict, typed_bytes=0,
                      elapsed_ms=round((time.monotonic() - started) * 1000, 1))
        ctx.produce("edit.json", json.dumps(record, indent=1).encode())
        ctx.fail(words)

    ctx.step("give Neovim the keys")
    view, rows = wait_rows(hand, *PANE, "", 10)
    ctx.check(rows, "Workshop does not describe the Editor pane; open it and switch it to Neovim")
    where = hand.point(*PANE, rows[0]["row"], 0, view["picture"])
    hand.inject([moment(ctx, "PointerButton", button=1, pressed=p, x=where["x"], y=where["y"],
                        space=where["space"]) for p in (True, False)])
    # After some events the top row is a notice ("editing <path>"); Escape gives the status back.
    hand.inject(chord_moments(ctx, "escape"))
    end = time.monotonic() + 3
    while status(hand).split(" ")[0] not in ("saved", "UNSAVED"):
        ctx.check(time.monotonic() < end, "the Editor pane's top row is not an editor status (it "
                  "reads %r)" % status(hand))
        time.sleep(0.1)
    mode = mode_of(status(hand))
    ctx.check(mode.replace("-", "").isalpha() and mode.isupper(), "the Editor pane is not Neovim's "
              "(its top row reads %r); switch the Editor to Neovim" % status(hand))
    # Escape leaves Insert, Visual and a command line. A terminal buffer keeps it, and there a
    # typed `:e` would be the terminal's input: nothing is typed until Neovim is in Normal mode.
    end = time.monotonic() + 3
    while mode_of(status(hand)) != "NORMAL":
        ctx.check(time.monotonic() < end, "Neovim is in %s mode and Escape did not bring it to "
                  "Normal mode; nothing was typed" % mode_of(status(hand)))
        time.sleep(0.1)

    ctx.step("open the file and ask Neovim what it holds")
    hand.inject([moment(ctx, "TextEntered", text=":e %s\n" % posix)])
    opened = ask(ctx, hand, posix, "after :e")
    record["opened"] = opened
    if not opened["same"]:
        refuse("NOT OPENED", "Neovim did not open %s: its current buffer is another file "
               "(Workshop's row reads %r). Neovim refuses :e while the current buffer holds unsaved "
               "changes and 'hidden' is off. Nothing was typed into any buffer"
               % (posix, opened["row"]))
    if opened["modified"]:
        refuse("UNSAVED", "Neovim holds unsaved changes to %s%s: save them (:w) or discard them "
               "(:e!) in Neovim first. Nothing was typed into it"
               % (posix, "" if existed else ", a document never saved (the file is not on disk)"))
    if opened["digest"] != record["disk_digest"]:
        refuse("NOT THE FILE", "Neovim's buffer for %s is not the file on disk (%s; the buffer "
               "holds %d line(s)): reload it (:e!) if the disk is right. Nothing was typed into it"
               % (posix, "%d line(s)" % len(lines) if existed else "the file is not on disk",
                  opened["lines"]))
    # 'fileformat' is always unix: a file with no line ending at all is otherwise written as dos
    # on Windows (Neovim's first 'fileformats' entry), and CRLF files are refused unless to_unix.
    settings = ":set paste\n:setlocal noautoindent nosmartindent nocindent indentexpr= noexpandtab"
    hand.inject([moment(ctx, "TextEntered", text=settings + " ff=unix\n")])

    ctx.step("type %d edit(s) as %d step(s)" % (len(edits), len(steps)))
    typed = 0
    for kind, value in steps:
        if kind == "press":
            hand.inject(chord_moments(ctx, value))
        elif kind == "keys":
            hand.inject([moment(ctx, "TextEntered", text=value)])
        else:
            text = "\n".join(value)
            while text:
                piece = text[:chunk]
                if len(text) > chunk and "\n" in piece:
                    piece = piece[:piece.rindex("\n") + 1]
                hand.inject([moment(ctx, "TextEntered", text=piece)])
                typed += len(piece.encode("utf-8"))
                text = text[len(piece):]
                time.sleep(pause + len(piece) / 50000.0)

    ctx.step("write, then ask Neovim what it saved and compare the file")
    # A new file's directories are made by Neovim's own `:w ++p`; an existing file is written as is.
    hand.inject(chord_moments(ctx, "escape") +
                [moment(ctx, "TextEntered", text=":w\n" if existed else ":w ++p\n")])
    saved = ask(ctx, hand, posix, "after :w")
    after = path.read_bytes() if path.is_file() else b""
    record.update(typed_bytes=typed, steps=len(steps), saved=saved, row=saved["row"],
                  after_sha256=hashlib.sha256(after).hexdigest(),
                  elapsed_ms=round((time.monotonic() - started) * 1000, 1))
    kept = saved["same"] and not saved["modified"] and saved["digest"] == record["planned_digest"]
    record["verdict"] = "matches" if kept and after == expected else (
        "DIFFERS" if after != expected else "NOT SAVED")
    ctx.produce("edit.json", json.dumps(record, indent=1).encode())
    if after != expected:
        ctx.produce("expected.txt", expected)
        ctx.produce("actual.txt", after)
        got, want = after.decode("utf-8", "replace").split("\n"), expected.decode("utf-8").split("\n")
        first = next((n for n in range(max(len(got), len(want)))
                      if n >= len(got) or n >= len(want) or got[n] != want[n]), None)
        ctx.fail("%s differs from the edits' text from line %s (Neovim's row: %r)"
                 % (posix, None if first is None else first + 1, saved["row"]))
    ctx.check(saved["same"], "after :w Neovim's current buffer is not %s (its row reads %r)"
              % (posix, saved["row"]))
    ctx.check(not saved["modified"], "Neovim did not save %s: the buffer is still modified though "
              "the file on disk is the planned text (its row reads %r)" % (posix, saved["row"]))
    ctx.check(saved["digest"] == record["planned_digest"], "Neovim's buffer for %s holds other text "
              "than the plan (%d line(s)) though the file on disk is the planned text"
              % (posix, saved["lines"]))
    return "%s: %d edit(s), %d bytes typed, saved and identical to the planned text (%d lines)" % (
        posix, len(edits), typed, len(final))
