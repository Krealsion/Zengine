# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""desk/watch -- follow a running Workshop's desk as it moves, and write each change for a model,
into the one directory `out` names.

Workshop says when its desk moved: `DeskStamps`, the desk number and every presented pane's stamp,
published at the end of each delivery that moved either. Each notice is the whole state, so the
relay may keep only the newest while this reader is behind (`latest`), and the newest stands for
every one before it. This reader asks nothing until it is told.

In order: `out` is judged as desk/read judges it, and AGENTS.md is written first; the notice is
followed (subscribed before the first read, so nothing between the read and the subscription is
missed; its window one notice, so while the watch reads, the relay holds only the newest); the
desk is read whole and its readings written. Then, at each notice -- the newest of those waiting --
what moved is read again: the desk, when its number moved or a pane came or left, every pane with
it, since a pane's place and what covers it are the desk's (a desk reading begun at most every
DESK_READ_GAP seconds, each confirming itself as desk/read does: Workshop answers each link's
session a few a second, and a reader beside this one needs some); else each pane whose stamp moved, alone. The new reading is compared with the
last, and the change written to `changes.txt` (desk_changes), the reading files rewritten. A notice
the last reading already stands on writes nothing, and a desk read refused writes nothing and is
asked again at the next notice. A gap -- notices the relay or this host lost -- reads the desk again
whole and says the gap. It ends when `seconds` pass, after `changes` changes, or when the notice's
subscription ends, and says which.

ONE WATCH A DIRECTORY. A watch holds `out` by a file of its own, `.desk-watch`, made only where none
stands and removed when it ends; a second watch of one `out` is refused, naming the first. A watch
continuing a ring a last watch left opens with a change saying so: what moved while nothing watched
is not said, and this watch's first reading is what it compares with.

A WATCH OF SOME PANES (`panes`) writes their changes and their files only: it is watching them, not
the desk. It keeps the guide, and removes the whole desk's files, so nothing it does not keep
current stands beside what it does.
"""

import json
import os
import time

from loom_session.tool import DispatchRefused, LinkOutcome, Refused, SendRefused

import desk_asks
import desk_changes as changes
import desk_words as words
import read

NOTICE = ("DeskStamps", 1)
#: How long a watch follows the desk, in seconds, by default and at most.
DEFAULT_SECONDS = 60.0
MAX_SECONDS = 3600.0
#: The least time between the starts of two desk readings of one watch, in seconds (a reading
#: confirms itself with a desk read or two more, as desk/read does): Workshop answers a link's
#: session four a second, every run on the link together, and a reader beside the watch needs some.
DESK_READ_GAP = 1.0
#: The file a watch holds its directory by.
HELD = ".desk-watch"


def watched_of(ctx, given):
    """The panes a watch of some panes writes, as a set of (office, pane); None for the desk."""
    text = (given or "").strip()
    if not text:
        return None
    try:
        refs = json.loads(text)
    except ValueError as err:
        ctx.fail("refused: `panes` is not JSON (%s): name each pane as [\"<office>\", \"<pane>\"]"
                 % err)
    ctx.check(isinstance(refs, list) and refs and all(
        isinstance(r, list) and len(r) == 2 and all(isinstance(p, str) and p for p in r)
        for r in refs),
        "refused: `panes` is a JSON list of [\"<office>\", \"<pane>\"] pairs, each two names")
    return set((r[0], r[1]) for r in refs)


def stamps_of(reading):
    """{(office, pane): the stamp each pane's reading stands on, or the desk read named}."""
    out = {}
    for read_one in (reading.get("panes") or {}).values():
        view = read_one.get("view")
        stamp = desk_asks.stamp_of(view) if view is not None else read_one.get("desk_stamp")
        if stamp is not None:
            out[(stamp.get("provider", ""), stamp.get("pane", ""))] = stamp
    return out


def moved(reading, notice):
    """What a notice says moved since `reading`: (whether the desk moved, [(office, pane)] whose
    stamp moved). Nothing, for a notice the reading already stands on or one older than it."""
    if reading.get("desk") is None:
        return True, []
    number = desk_asks.desk_number({"desk": reading["desk"]})
    told = notice.get("desk", 0)
    if told < number:
        return False, []
    known = stamps_of(reading)
    said = dict(((s.get("provider", ""), s.get("pane", "")), s) for s in notice.get("panes") or [])
    if told != number or set(said) != set(known):
        return True, []
    return False, [ref for ref, s in said.items() if not desk_asks.same_stamp(s, known[ref])]


class Watch(object):
    """One watch's reading and its asks, paced."""

    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.asker = desk_asks.Asker(ctx, link)
        self.last_desk_read = None

    def read_desk(self, kept):
        """The desk read whole again, paced, keeping `kept`'s row, inventory and keymap."""
        if self.last_desk_read is not None:
            wait = DESK_READ_GAP - (time.monotonic() - self.last_desk_read)
            if wait > 0:
                time.sleep(wait)
        reading = desk_asks.read_desk(self.asker)
        self.last_desk_read = time.monotonic()
        for k in ("row", "row_refused", "inventory", "inventory_refused", "keymap",
                  "keymap_refused"):
            reading[k] = kept.get(k)
        reading["asks"] = self.asker.asks
        return reading

    def read_panes(self, reading, refs, notice):
        """`reading` with each pane of `refs` read again alone. The desk did not move, and the
        notice names each pane's stamp as a desk read would now: the stamp its reading is said
        against."""
        out = dict(reading)
        panes = dict(reading.get("panes") or {})
        said = dict(((s.get("provider", ""), s.get("pane", "")), s)
                    for s in notice.get("panes") or [])
        for ref in refs:
            key = desk_asks.key(*ref)
            again = desk_asks.read_pane(self.asker, ref[0], ref[1])
            again["desk_stamp"] = said.get(ref) or (panes.get(key) or {}).get("desk_stamp")
            panes[key] = again
        out["panes"] = panes
        out["asks"] = self.asker.asks
        return out


def kept(write):
    """Whether `write` wrote: a file a reader holds open on Windows may refuse it past
    `read.REPLACE_RETRY`, and the watch writes it at its next change instead of ending."""
    try:
        write()
        return True
    except PermissionError:
        return False


def write_readings(out, reading, watched):
    """The reading files: every one, as desk/read writes them, or a watch of some panes' own and
    the guide."""
    if watched is None:
        written = read.readings(reading)
    else:
        written = [(rel, text) for rel, text in read.readings(reading)
                   if rel.startswith("guide")] + read.readings(reading, only=watched)
    for rel, text in written:
        read.write(os.path.join(out, *rel.split("/")), text)
    read.tidy(out, written, desk_files=watched is not None)


def hold(ctx, out):
    """`out` held for this watch, or refused in words naming the watch that holds it."""
    path = os.path.join(out, HELD)
    try:
        fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    except FileExistsError:
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                holder = f.read().strip() or "an unnamed run"
        except OSError:
            holder = "a run whose name could not be read"
        ctx.fail("refused: `out` (%s) is held by another desk/watch, %s: one watch a directory. "
                 "Name another, or remove %s if no watch runs there" % (out, holder, path))
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("desk/watch run %s\n" % ctx.name)
    ctx.on_cleanup(lambda: os.path.exists(path) and os.remove(path),
                   "release the watch's hold on %s" % out)
    return path


def ring_of(path):
    if not os.path.isfile(path) or os.path.islink(path):
        return changes.Ring()
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return changes.Ring.parse(f.read())


def run(ctx):
    ctx.step("out")
    out = read.output_dir(ctx, ctx.inputs.get("out"))
    link = ctx.inputs.get("link") or "workshop"
    seconds = min(MAX_SECONDS, max(0.0, float(ctx.inputs.get("seconds", DEFAULT_SECONDS) or 0)))
    wanted = max(0, int(ctx.inputs.get("changes", 0) or 0))
    watched = watched_of(ctx, ctx.inputs.get("panes"))
    os.makedirs(os.path.join(out, "panes"), exist_ok=True)
    held = hold(ctx, out)
    ctx.step("AGENTS.md")
    agents = os.path.join(out, "AGENTS.md")
    read.write(agents, words.AGENTS_MD)  # first, and before any ask: the reader's own words only
    ctx.step("follow")
    try:
        sub = ctx.observe(desk_asks.WORKSHOP, [NOTICE], via=link, latest=[NOTICE[0]], window=1,
                          label="desk/watch (run %s)" % ctx.name)
    except (Refused, DispatchRefused, SendRefused, LinkOutcome) as err:
        ctx.fail("refused: this row may not follow the desk: %s. A row follows it with `capture` "
                 "and an `observe` entry naming zengine.workshop DeskStamps 1"
                 % desk_asks.refusal_words(err))
    ctx.step("read")
    watch = Watch(ctx, link)
    reading = desk_asks.read_all(ctx, link)
    watch.last_desk_read = time.monotonic()
    blocked = 0 if kept(lambda: write_readings(out, reading, watched)) else 1
    ring_path = os.path.join(out, "changes.txt")
    ring = ring_of(ring_path)
    if ring.restarted:
        ring.add("the ring starts again: changes.txt was not one this reader wrote", [])
    elif ring.entries:
        ring.add("a new watch: what moved while nothing watched is not said; this watch compares "
                 "with its own first reading", [])
    first = ring.next_number()
    ctx.step("watch")
    began = time.monotonic()
    notices = gaps = lost = quiet = written = refused = 0
    why = "%g seconds passed" % seconds
    while True:
        left = seconds - (time.monotonic() - began)
        if left <= 0:
            break
        if wanted and written >= wanted:
            why = "%d change(s) were written" % written
            break
        item = sub.next(left)
        if item is None:
            if sub.ended is not None:
                why = "the notice's subscription ended (%s)" % sub.ended.record().get("how", "?")
                break
            continue
        batch = [item] + sub.drain()
        told = [i for i in batch if i.kind == "observed"]
        holes = [i for i in batch if i.kind == "gap"]
        ended = next((i for i in batch if i.kind == "ended"), None)
        notices += len(told)
        title = None
        if holes:
            gaps += len(holes)
            lost += sum(i.lost for i in holes)
            new = watch.read_desk(reading)
            title = "after a gap of %d lost notice(s), the desk read again whole" % (
                sum(i.lost for i in holes))
        elif told:
            notice = told[-1].fields
            desk_moved, panes_moved = moved(reading, notice)
            if watched is not None:
                panes_moved = [ref for ref in panes_moved if ref in watched]
            if desk_moved:
                new = watch.read_desk(reading)
            elif panes_moved:
                new = watch.read_panes(reading, panes_moved, notice)
            else:
                new = None
                quiet += 1
            if new is not None:
                title = "notice %d: desk %d" % (told[-1].seq, notice.get("desk", 0))
        else:
            new = None
        if new is not None and new.get("desk") is None:
            # A DESK READ REFUSED -- its rate, or the row -- is no change: the reading stands, and
            # the next notice asks again.
            refused += 1
            new = None
        if new is not None:
            lines = changes.change(reading, new, watched)
            if lines:
                ring.add(title, lines)
                blocked += not kept(lambda: read.write(ring_path, ring.text()))
                written += 1
            elif not holes:
                quiet += 1
            blocked += not kept(lambda: write_readings(out, new, watched))
            reading = new
        if ended is not None:
            why = "the notice's subscription ended (%s)" % ended.how
            break
    if sub.ended is None:
        sub.release()
    if written or ring.restarted:
        blocked += not kept(lambda: read.write(ring_path, ring.text()))
    os.remove(held)
    return result(agents, out, seconds, time.monotonic() - began, notices, written, first, quiet,
                  gaps, lost, refused, why, watched, blocked)


def result(agents, out, seconds, spent, notices, written, first, quiet, gaps, lost, refused, why,
           watched, blocked=0):
    line = "Read %s first. desk/watch followed the desk in %s for %.1f s: %d notice(s), " % (
        agents, out, spent, notices)
    if written:
        line += "%d change(s) written to changes.txt (changes %d to %d)" % (
            written, first, first + written - 1)
    else:
        line += "no change written"
    if quiet:
        line += ", %d notice(s) that moved nothing it writes" % quiet
    if gaps:
        line += ", %d gap(s) losing %d notice(s), each read again whole" % (gaps, lost)
    if refused:
        line += ", %d desk read(s) refused and asked again at the next notice" % refused
    if blocked:
        line += (", %d write(s) refused while a reader held a file open, each written again at "
                 "the next change" % blocked)
    if watched is not None:
        line += "; it watched %d pane(s) and wrote only theirs" % len(watched)
    return (line + "; it ended as %s" % why)[:1900]
