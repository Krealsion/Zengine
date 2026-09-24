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
made after Neovim's pane says the buffer is saved.

TYPED TEXT STAYS LITERAL. A buffer's indent script can turn 'autoindent' back on after 'paste'
(Python's does), which stacks each line's indentation onto the next. After opening the file the
tool sets 'paste' again and clears autoindent, smartindent, cindent, indentexpr and expandtab for
that buffer. A new file is written with Unix line endings.

NEOVIM READS ITS INPUT LATER. Workshop's settle fence covers Workshop's own bus, not Neovim's input
queue, so the tool types in bounded chunks with a pause, and waits on what the pane paints (the
file named, `saved`) and on the file itself rather than on an injection's answer.

WHAT IT REFUSES. A path Ex would have to escape, a CRLF file, a buffer with unsaved changes (the
tool would not know which text is meant), an anchor that matches no line or several. It does not
undo: a failed run says what Neovim was asked to do and leaves the buffer as it is."""
import hashlib
import json
import time
from pathlib import Path

from act import painted, wait_rows
from hand import Hand
from workshop_steps import chord_moments, moment

PANE = ("zengine.editor", "editor")
KINDS = ("create", "rewrite", "append", "after", "before", "replace", "delete")


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


def status(hand):
    """Neovim's pane's top row -- Workshop's account of it: saved or UNSAVED, mode, profile, file."""
    view = painted(hand, *PANE)
    return view["rows"][0]["text"] if view and view["rows"] else ""


def run(ctx):
    path = Path(ctx.inputs["path"])
    posix = path.as_posix()
    ctx.check(path.is_absolute(), "path must be absolute: Neovim's working directory is Workshop's")
    ctx.check(not any(c in posix for c in " %#|\"'\\"), "the path holds a character Ex would "
              "read as something else; move the file or open it by hand")
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
              "expected_sha256": hashlib.sha256(expected).hexdigest(), "edits": spans}
    started = time.monotonic()
    hand = Hand(ctx, ctx.inputs["link"])

    ctx.step("give Neovim the keys and open the file")
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
    mode = (status(hand).split(" ") + [""])[1]
    ctx.check(mode.replace("-", "").isalpha() and mode.isupper(), "the Editor pane is not Neovim's "
              "(its top row reads %r); switch the Editor to Neovim" % status(hand))
    hand.inject([moment(ctx, "TextEntered", text=":e %s\n" % posix)])
    tail = posix[-24:]  # the row names the file cut from the left to the pane's width
    end = time.monotonic() + 15
    while not status(hand).endswith(tail):
        ctx.check(time.monotonic() < end, "Neovim did not open %s (its row reads %r)"
                  % (posix, status(hand)))
        time.sleep(0.1)
    ctx.check(not existed or status(hand).startswith("saved"), "Neovim holds unsaved changes to "
              "%s; save or discard them first (its row reads %r)" % (posix, status(hand)))
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

    ctx.step("write and compare")
    # A new file's directories are made by Neovim's own `:w ++p`; an existing file is written as is.
    hand.inject(chord_moments(ctx, "escape") +
                [moment(ctx, "TextEntered", text=":w\n" if existed else ":w ++p\n")])
    end = time.monotonic() + 20
    after = b""
    while time.monotonic() < end:
        after = path.read_bytes() if path.is_file() else b""
        if status(hand).startswith("saved") and after == expected:
            break
        time.sleep(0.2)
    record.update(typed_bytes=typed, steps=len(steps), after_sha256=hashlib.sha256(after).hexdigest(),
                  row=status(hand), elapsed_ms=round((time.monotonic() - started) * 1000, 1))
    record["verdict"] = "matches" if after == expected else "DIFFERS"
    ctx.produce("edit.json", json.dumps(record, indent=1).encode())
    if after != expected:
        ctx.produce("expected.txt", expected)
        ctx.produce("actual.txt", after)
        got, want = after.decode("utf-8", "replace").split("\n"), expected.decode("utf-8").split("\n")
        first = next((n for n in range(max(len(got), len(want)))
                      if n >= len(got) or n >= len(want) or got[n] != want[n]), None)
        ctx.fail("%s differs from the edits' text from line %s (Neovim's row: %r)"
                 % (posix, None if first is None else first + 1, status(hand)))
    return "%s: %d edit(s), %d bytes typed, saved and identical to the planned text (%d lines)" % (
        posix, len(edits), typed, len(final))
