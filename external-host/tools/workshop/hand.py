# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Small orchestration helpers. Zengine owns geometry, motion, drops and permissions.

A pane is read by its words (PaneView version 3) wherever a reader finds a row by what it says or
a part by its name -- `row`, `row_starting`, `part`, `last_part`, `first`, `control`, `spot`,
`field`, `rows_of` -- so a text pane and a pane that draws a canvas picture read alike: a text
pane's word is its row, a canvas pane's each text run or label it drew, and a row it leaves blank
is no word. `point` and `lattice_point` name one cell of a pane's text lattice by its row and
column (PanePoint, asked by version 3): a text pane's painted cell, or the cell of the lattice a
canvas pane sets its text on, a blank row's and the one after a row's last character too. `view`
reads a text pane's rows by number (version 1), which a canvas pane does not answer.

A canvas picture that takes no press -- the one a managed opening shows, until its pane draws its
own, or one its office's holder no longer holds -- says its words and parts with no point
(`takes_no_press`), and refuses a point asked of it; every helper here that presses reads the pane
again until one does (`pressing`).

A POINT IS ASKED OF THE PICTURE ITS READING READ (`Hand.aimed`), and Workshop refuses one where the
pane drew another since: the pane is read and aimed again, at most POINT_TRIES times -- or, while
that newer picture, a canvas pane's or a text pane's, is not yet aimed at (`no_settled_picture`),
once it may have settled, for at most the reader's seconds. `row`, `pointed` and `lattice_point`
aim so, and every helper built on them.

A WAIT IS FOR THE DESK TO MOVE. Workshop says when its desk moved (`DeskStamps` v2, the desk
number, every presented pane's stamp and the picture each is aimed at), so a helper waiting for a
pane to say something reads it again when that pane's stamp, its aimed picture or the desk number
moved, not on a clock (`Notices`, `Hand.wait`) -- a picture whose number is not yet aimed at
(`no_settled_picture`) included, which its aimed number moving marks. A row whose `observe` names
`DeskStamps` 1 and not 2 follows the first version, which names no aimed picture: an unsettled
picture is read again every FALLBACK_PACE seconds, and the run says so. A row naming neither is
read again every FALLBACK_PACE seconds at every wait, and the run says so too."""
import json
import time
from collections import Counter

from loom_session.tool import DispatchRefused, LinkOutcome, Refused, SendRefused

from workshop_steps import moment, chord_moments, clear_moments


def on_row(word, part):
    """Whether a word stands on the row a part stands on: their places overlap from top to bottom.
    A text pane's row part and its row's word share one place; a canvas pane's text run stands on
    its lattice's row, which the part's rectangle covers."""
    w, p = word["place"], part["place"]
    return w["y"] < p["y"] + p["h"] and p["y"] < w["y"] + w["h"]


def word_on_row(view, part):
    """The leftmost word standing on the row a part stands on, or None."""
    rows = [w for w in view["words"] if on_row(w, part)] if view else []
    return min(rows, key=lambda w: w["place"]["x"]) if rows else None


def topmost(words):
    """The leftmost of the words on the highest row among `words`, or None."""
    return min(words, key=lambda w: (w["place"]["y"], w["place"]["x"])) if words else None


#: Why a pane's picture took no press, as a tool says it when its wait runs out.
NO_PRESS = ("Workshop gave its words and parts no point, as it does at the picture a managed opening "
            "shows until its pane draws its own, or at one its office's holder no longer holds")


def takes_no_press(view):
    """Whether a pane's picture takes no press now: a canvas pane saying every word and part it
    draws with no point -- 0, 0 in no space -- as Workshop says the picture a managed opening shows
    until its pane draws its own, or one its office's holder no longer holds. A press there reaches
    nothing; the pane is read again."""
    said = (view["words"] + view.get("parts", [])) if view else []
    return bool(view) and bool(view.get("canvas")) and bool(said) and all(
        s.get("space", 0) == 0 for s in said)


#: How many readings a point is aimed from while Workshop refuses it because the pane's picture
#: moved between the reading and the point; past it the aiming fails naming the pane.
POINT_TRIES = 3
#: How long, in seconds, a reader waits for a pane's picture to settle: a reading's (`pressing`),
#: and a point's while the picture it would land on is not yet aimed at (`Hand.aimed`).
SETTLE_SECONDS = 10


def no_settled_picture(refused):
    """Whether Workshop refused a reading or a point because the pane holds no settled picture: its
    newest one -- a canvas pane's, or a text pane's, as each refusal says -- not yet aimed at. The
    pane's aimed number moving ends it (`Hand.wait`, `unsettled`)."""
    said = str(refused)
    return "no settled picture" in said or "no settled text picture" in said


def rows_of(view):
    """A pane's words in the shape a reader of rows takes: `{picture, canvas, rows}`, each row
    `{row, text, x, y, space}` -- `row` the word's number, which is a text pane's row; `text` its
    characters without the blanks after the last; `x, y, space` the point a press names it by."""
    return {"provider": view["provider"], "pane": view["pane"], "picture": view["picture"],
            "canvas": view.get("canvas", False),
            "rows": [{"row": w["word"], "text": w["text"], "x": w["x"], "y": w["y"],
                      "space": w["space"]} for w in view["words"]]}


#: The notice Workshop publishes when its desk moved, and the first version, which a row naming
#: only it follows: it names no aimed picture.
DESK_NOTICE = ("DeskStamps", 2)
DESK_NOTICE_V1 = ("DeskStamps", 1)
#: How often a wait reads again when the notice cannot be followed, in seconds.
FALLBACK_PACE = 0.2
#: The notices a hand's subscription may hold untaken: one acknowledgement serves half of them.
NOTICE_WINDOW = 8


class Notices:
    """Workshop's notices that the desk moved, followed through the link for one hand: a window of
    NOTICE_WINDOW, the relay keeping only the newest once it is full. Subscribed at the first wait,
    which returns at once so its caller reads again under the subscription; released when the hand
    closes. Every wait that reads again on a clock is counted (`clock_waits`)."""

    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.sub = None
        self.why = None      # why notices are not followed, once found: the waits read on a clock
        self.unaimed = None  # why an unsettled picture is read on a clock: the first version
        self.seen = None     # the newest notice taken: (desk number, {(office, pane): stamp})
        self.clock_waits = 0

    def follow(self):
        observe = getattr(self.ctx, "observe", None)
        if observe is None:
            self.why = "this run's context follows no publication"
            return
        refused = []
        for notice in (DESK_NOTICE, DESK_NOTICE_V1):
            try:
                self.sub = observe("zengine.workshop", [notice], via=self.link, latest=[notice[0]],
                                   window=NOTICE_WINDOW, label="a hand's waits")
            except (Refused, DispatchRefused, SendRefused, LinkOutcome) as err:
                refused.append(str(err))
                continue
            if notice == DESK_NOTICE_V1:
                self.unaimed = ("the row follows DeskStamps 1, which names no aimed picture: %s"
                                % refused[0])
            return
        self.why = "Workshop's notice could not be followed: %s" % "; ".join(refused)

    def moved(self, notice, pane):
        """Whether `notice` moved what a wait for `pane` waits on since the last one taken: the
        desk number, or `pane`'s stamp and aimed picture (every pane's, for a wait for the desk).
        A second-version entry is `{stamp, aimed}`; a first-version one, the stamp alone."""
        said = {}
        for entry in notice.get("panes") or []:
            s = entry.get("stamp", entry)
            said[(s.get("provider", ""), s.get("pane", ""))] = (
                s.get("holder"), s.get("incarnation"), s.get("grant"), s.get("fingerprint"),
                entry.get("aimed"))
        before, self.seen = self.seen, (notice.get("desk", 0), said)
        if before is None or before[0] != self.seen[0]:
            return True
        if pane is None:
            return before[1] != said
        return before[1].get(pane) != said.get(pane)

    def wait(self, seconds, pane=None, unsettled=False):
        """Until the desk moves -- for `pane` (office, pane), its stamp, its aimed picture or the
        desk number; for none, any of it -- or `seconds` pass. `unsettled`: the wait is for a
        picture to be aimed at, which only the second version's notice marks."""
        if self.sub is None and self.why is None:
            self.follow()
            if self.sub is not None:
                return
        if self.sub is None or (unsettled and self.unaimed is not None):
            self.clock_waits += 1
            time.sleep(max(0.0, min(seconds, FALLBACK_PACE)))
            return
        end = time.monotonic() + max(0.0, seconds)
        while True:
            item = self.sub.next(max(0.0, end - time.monotonic()))
            items = ([item] if item is not None else []) + self.sub.drain()
            woke = False
            for i in items:
                if i.kind == "observed":
                    # ...and one standing for notices the relay held back may hide a move and its
                    # return to a picture told before: it wakes the wait whatever it names.
                    moved = self.moved(i.fields, pane)
                    woke = moved or getattr(i, "coalesced", 0) > 0 or woke
                else:  # a gap, or the subscription's end: read again, and on a clock after an end
                    woke = True
                    if i.kind == "ended":
                        self.why, self.sub = "Workshop's notice ended: %s" % i.how, None
                        break
            if woke or self.sub is None or time.monotonic() >= end:
                return

    def close(self):
        if self.sub is not None:
            self.sub.release()
            self.sub = None


class Hand:
    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.actions = Counter()  # weaver gestures this hand made, by kind
        self.session = self.ask("zengine.input", "InputSessionRequested",
                                {"purpose": "inventory composition demo"})["session"]
        self.open = True
        self.unsettled = False
        self.notices = Notices(ctx, link)
        ctx.on_cleanup(self.close, "release the demo input session")

    def ask(self, role, shape, fields, **kw):
        return self.ctx.ask(role, shape, fields, via=self.link, **kw)

    def close(self):
        try:  # the input session first: a release refused still gives it back
            if self.open:
                self.ask("zengine.input", "InputSessionClosed", {"session": self.session, "holder": 0})
                self.open = False
        finally:
            self.notices.close()

    def wait(self, seconds, pane=None, unsettled=False):
        """Until the desk moves, for `pane` (office, pane) or for the desk, or `seconds` pass
        (`Notices.wait`); `unsettled`, for a picture to be aimed at."""
        self.notices.wait(seconds, pane, unsettled)

    def waits_said(self):
        """How this hand's waits waited, in words, when not by Workshop's notice; else None."""
        return self.notices.why or self.notices.unaimed

    def clock_waits(self):
        """How many of this hand's waits read again on a clock, not at Workshop's notice."""
        return self.notices.clock_waits

    def inject(self, events):
        """Inject and settle; returns the Input owner's answer, whose ``correlation`` is this
        run's number for the press -- what an observation's ``cause`` names when the press set
        it in motion (``ctx.observe``)."""
        answer = self.ask("zengine.input", "InjectInput",
                          {"session": self.session, "events": events}, settle=True)
        self.ctx.check(answer["session"] == self.session and answer["admitted"] == len(events),
                       "input did not admit the requested moments")
        return answer

    def view(self, provider, pane):
        """A text pane's rows by number (PaneView version 1); a pane that draws a picture is
        refused. `point` takes its picture from `words`, which reads either."""
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    def words(self, provider, pane):
        """A pane's words and named parts as Workshop holds them (PaneView version 3): a text pane's
        rows or a canvas pane's labels and runs, each with its place in canvas pixels and the point a
        press names it by, and beside them every part the pane names, under the pane's own name.
        `unsettled` says whether the last such read was refused for a picture not yet aimed at."""
        try:
            view = self.ask("zengine.workshop", "PaneViewRequested",
                            {"provider": provider, "pane": pane}, version=3)
        except Refused as refused:
            self.unsettled = no_settled_picture(refused)
            raise
        self.unsettled = False
        return view

    def pressing(self, provider, pane, seconds=SETTLE_SECONDS):
        """The pane's words and parts, as `words` reads them, once its picture takes a press: while
        it takes none (`takes_no_press`), or Workshop calls its picture unsettled
        (`no_settled_picture`) -- as it does while the pane's own picture settles after one -- the
        pane is read again, for at most `seconds`; then ValueError, keeping `last-view.json`, or the
        refusal. Any other refusal -- a closed, covered or overlapped pane -- is raised at once, as
        `words` raises it."""
        end = time.monotonic() + seconds
        while True:
            try:
                view = self.words(provider, pane)
            except Refused as refused:
                if not no_settled_picture(refused) or time.monotonic() >= end:
                    raise
                # ...UNTIL ITS NUMBER IS AIMED AT, which moves the pane's aimed picture.
                self.wait(end - time.monotonic(), (provider, pane), unsettled=True)
                continue
            if not takes_no_press(view):
                return view
            if time.monotonic() >= end:
                self.last_view(view)
                raise ValueError("%s/%s's picture took no press within %gs: %s"
                                 % (provider, pane, seconds, NO_PRESS))
            # THE PICTURE THAT FIRST TAKES A PRESS MOVES THE PANE'S STAMP, so it is waited for.
            self.wait(end - time.monotonic(), (provider, pane))

    def word_point(self, provider, pane, word, column, picture):
        """Where one character of one of those words is now; refused if the picture moved."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "word": word, "column": column}, version=2)

    def desk(self):
        """The desk by Workshop's own numbers (DeskView version 2): every pane on it with its state,
        rank from the front, place and size, selection and keys; the room; arranging; the menu, and
        the lines it names. A desk read settles nothing a pane read left `unsettled`: a wait on the
        desk waits on its notice."""
        self.unsettled = False
        return self.ask("zengine.workshop", "DeskViewRequested", {}, version=2)

    def seek(self, provider, pane, find, scroll, toward=(1, -1), notches=256):
        """Read the pane's words until `find(view)` names something, searching toward each edge
        with ordinary wheel input when `scroll`: one notch at a time at the first word's point, in
        each of the wheel directions `toward` in turn (1 away from the weaver, -1 toward), at most
        `notches` each, until the words stop changing -- a stable view is the edge, not a reason to
        retry it. Returns `(view, found)`, `found` None when no view named anything."""
        view = None
        for direction in (toward if scroll else (0,)):
            previous = None
            for _ in range(notches):
                view = self.pressing(provider, pane)
                found = find(view)
                if found is not None:
                    return view, found
                visible = [(w["word"], w["text"]) for w in view["words"]]
                if not direction or not visible or visible == previous:
                    break
                previous = visible
                at = view["words"][0]
                self.actions["wheel"] += 1
                self.inject([moment(self.ctx, "PointerWheel", wheel_dy=direction,
                                    x=at["x"], y=at["y"], space=at["space"])])
        return view, None

    def last_view(self, view, kept="last-view.json"):
        """Keep what the pane last said -- its rows as `rows_of` reads them, and the names of its
        parts -- as `kept`: for a reader of a failed search, or as a run's evidence."""
        if view is not None:
            self.ctx.produce(kept, json.dumps(dict(rows_of(view), parts=[
                p["name"] for p in view.get("parts", [])]), indent=2).encode())

    def aimed(self, provider, pane, read, aim, what, seconds=SETTLE_SECONDS):
        """`(reading, point)`: `read()` reads the pane, `aim(reading)` asks Workshop where to press
        on that reading. A point refused because the pane's picture moved since the reading is read
        and aimed again, at most POINT_TRIES times; then ValueError naming `what`. One refused
        because the pane holds no settled picture -- a newer one, not yet aimed at -- is read and
        aimed again once that picture may have settled (at its aimed number moving, `wait`), for at
        most `seconds`; then that refusal is raised. Any other refusal is raised at once."""
        end = time.monotonic() + seconds
        moved = 0
        while True:
            reading = read()
            try:
                return reading, aim(reading)
            except Refused as refused:
                if no_settled_picture(refused):
                    if time.monotonic() >= end:
                        raise
                    self.wait(end - time.monotonic(), (provider, pane), unsettled=True)
                    continue
                if "picture moved" not in str(refused):
                    raise
                moved += 1
                if moved >= POINT_TRIES:
                    raise ValueError("%s in %s/%s kept moving while it was read"
                                     % (what, provider, pane))

    def row(self, provider, pane, contains, scroll=False, seconds=SETTLE_SECONDS):
        """The one visible row holding `contains` -- a text pane's row, or the one run of text a
        canvas pane draws on a row -- as `{row, text, x, y, space}`, pressed where Workshop says its
        third character is (its own point where it has fewer), aimed as `aimed` aims, for at most
        `seconds`. With `scroll`, the pane is wheeled toward each edge until the row shows. Two
        rows holding it fail the run's check (the row is ambiguous); none raises ValueError and
        keeps `last-view.json`."""
        def find(view):
            found = [w for w in view["words"] if contains in w["text"]]
            self.ctx.check(len(found) <= 1, "ambiguous visible row: " + contains)
            return found[0] if found else None
        def read():
            view, w = self.seek(provider, pane, find, scroll)
            if w is None:
                self.last_view(view)
                raise ValueError("visible row not found: " + contains)
            return view, w
        def aim(reading):
            view, w = reading
            return w if len(w["text"]) < 3 else self.word_point(provider, pane, w["word"], 2,
                                                                view["picture"])
        (_, w), at = self.aimed(provider, pane, read, aim, "visible row %r" % contains, seconds)
        return {"row": w["word"], "text": w["text"], "x": at["x"], "y": at["y"],
                "space": at["space"]}

    def row_starting(self, provider, pane, prefix):
        """The topmost row starting with `prefix` -- a text pane's row, or the one run of text a
        canvas pane draws on a row -- as `{row, text, x, y, space}`, pressed where Workshop says its
        third character is, as `row` presses one (its own point where it has fewer): an Inventory
        view's first line inside a box, `|`, is pressed inside its first box, though the line
        crosses every box. A pane that redrew between the reading and the point is read again;
        none raises ValueError and keeps `last-view.json`."""
        def pick(view):
            return topmost([w for w in view["words"] if w["text"].startswith(prefix)])
        w, at = self.pointed(provider, pane, pick, lambda w: 2 if len(w["text"]) >= 3 else None,
                             "row starting with %r" % prefix)
        return {"row": w["word"], "text": w["text"], "x": at["x"], "y": at["y"],
                "space": at["space"]}

    def pointed(self, provider, pane, pick, column, what, seconds=SETTLE_SECONDS):
        """The word `pick(view)` chooses among the pane's words now and where Workshop says its
        character `column(word)` is -- the word's own point where that is None -- as `(word,
        point)`, aimed as `aimed` aims, for at most `seconds`: a pane that redrew between the
        reading and the point is read again, as a canvas pane numbers every picture it sends. No
        word chosen raises ValueError naming `what` and keeps `last-view.json`."""
        def read():
            view = self.pressing(provider, pane)
            w = pick(view)
            if w is None:
                self.last_view(view)
                raise ValueError("no visible %s in %s/%s" % (what, provider, pane))
            return view, w
        def aim(reading):
            view, w = reading
            at = column(w)
            return w if at is None else self.word_point(provider, pane, w["word"], at,
                                                        view["picture"])
        (_, w), at = self.aimed(provider, pane, read, aim, what, seconds)
        return w, at

    def part(self, provider, pane, name, scroll=False):
        """The part a pane names `name`, wherever its last redraw put it, as `{name, row, text, x,
        y, space}` -- the point Workshop gives it, `row` the number of the word on its row (None
        where its row draws none). With `scroll`, the pane is wheeled toward each edge until it
        draws the part. A part no press reaches on its own, or none by that name, raises
        ValueError and keeps `last-view.json`."""
        def find(view):
            parts = [p for p in view.get("parts", []) if p["name"] == name]
            return parts[0] if parts else None
        view, p = self.seek(provider, pane, find, scroll)
        return self.pressable(provider, pane, view, p, name)

    def last_part(self, provider, pane, named, says):
        """The lowest part a pane draws whose name starts with `named` and whose text with `says` --
        a transcript's newest entry saying it, as the Terminal names a value row `entry:<n>` -- as
        `part` gives one. None drawn, or the lowest no press reaches on its own, raises ValueError
        and keeps `last-view.json`."""
        def find(view):
            parts = [p for p in view.get("parts", []) if p["name"].startswith(named)
                     and p["text"].startswith(says)]
            return max(parts, key=lambda p: p["place"]["y"]) if parts else None
        view, p = self.seek(provider, pane, find, False)
        return self.pressable(provider, pane, view, p, "%s* saying %r" % (named, says))

    def pressable(self, provider, pane, view, p, name):
        """A part found in `view` as `{name, row, text, x, y, space}`; ValueError, keeping
        `last-view.json`, where there is none or no press reaches it on its own."""
        if p is None:
            self.last_view(view)
            raise ValueError("no visible part %s in %s/%s" % (name, provider, pane))
        if p.get("space", 0) == 0:
            self.last_view(view)
            raise ValueError("no press reaches %s in %s/%s on its own: the parts over it take "
                             "every place of it" % (name, provider, pane))
        word = word_on_row(view, p)
        return {"name": p["name"], "row": word["word"] if word is not None else None,
                "text": p["text"], "x": p["x"], "y": p["y"], "space": p["space"]}

    def first(self, provider, pane):
        """A pane's first word -- the leftmost on its top row -- as `{row, text, x, y, space}`, at
        the point Workshop gives it: where a press or a drop meant for the pane's first row lands.
        A pane drawing no word raises ValueError and keeps `last-view.json`."""
        view = self.pressing(provider, pane)
        w = topmost(view["words"])
        if w is None:
            self.last_view(view)
            raise ValueError("%s/%s draws no word" % (provider, pane))
        return {"row": w["word"], "text": w["text"], "x": w["x"], "y": w["y"], "space": w["space"]}

    def point(self, provider, pane, row, column, picture):
        """Where one cell of a pane's text lattice is now, by its row and column counted from 0
        (PanePointRequested version 3, answered as PanePoint): a text pane's painted cell, or the
        centre of the cell of the lattice a canvas pane sets its text on -- any cell of it, a blank
        row's and the one after a row's last character too. Refused if the picture moved, and past
        the lattice."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "row": row, "column": column}, version=3)

    def lattice_point(self, provider, pane, pick, what, seconds=SETTLE_SECONDS):
        """The cell `pick(view)` chooses from the pane's words now -- `(row, column)` of its text
        lattice, counted from 0 -- and where Workshop says it is (`point`), as `(cell, point)`,
        aimed as `aimed` aims, for at most `seconds`. No cell chosen raises ValueError naming
        `what` and keeps `last-view.json`."""
        def read():
            view = self.pressing(provider, pane)
            cell = pick(view)
            if cell is None:
                self.last_view(view)
                raise ValueError("no visible %s in %s/%s" % (what, provider, pane))
            return view, cell
        def aim(reading):
            view, cell = reading
            return self.point(provider, pane, cell[0], cell[1], view["picture"])
        (_, cell), at = self.aimed(provider, pane, read, aim, what, seconds)
        return cell, at

    def control(self, provider, pane, label):
        """Press a visible `[label]` control, in a text pane's rows or a canvas pane's words: the
        column comes from the painted word, the screen position from Workshop, so no caller
        multiplies a font metric. A pane that redrew between the reading and the point is read
        again, as a canvas pane numbers every picture it sends."""
        word = "[" + label + "]"
        def pick(view):
            found = [w for w in view["words"] if word in w["text"]]
            return found[0] if found else None
        _, where = self.pointed(provider, pane, pick, lambda w: w["text"].find(word) + 1,
                                "control " + word)
        self.click(where)
        return where

    def spot(self, provider, pane, text, row_prefix=""):
        """Where Workshop draws `text` now -- its second character -- on the topmost row starting
        with `row_prefix` that holds it, a text pane's row or the one run a canvas pane draws on a
        row: an Inventory location crumb, `[Up]`, a control or a folder name, measured by
        Workshop. A pane that redrew between the reading and the point is read again; `text`
        drawn on no such row raises ValueError and keeps `last-view.json`."""
        def pick(view):
            return topmost([w for w in view["words"]
                            if w["text"].startswith(row_prefix) and text in w["text"]])
        _, where = self.pointed(provider, pane, pick, lambda w: w["text"].find(text) + 1,
                                "%r on a row starting with %r" % (text, row_prefix))
        return where

    def field(self, provider, pane, label, notches=64):
        """An Info view's field `label` -- the row it names `field:<label>` -- as `{name, row,
        text, x, y, space}` at the point Workshop gives it, a press or a drop on the field. The
        view's selection is walked with the wheel until the view draws it, as its window follows
        the selection: toward later fields first, then earlier, at most `notches` each way. None
        drawn raises ValueError and keeps `last-view.json`."""
        name = "field:" + label
        def find(view):
            parts = [p for p in view.get("parts", []) if p["name"] == name]
            return parts[0] if parts else None
        view, p = self.seek(provider, pane, find, True, toward=(-1, 1), notches=notches)
        return self.pressable(provider, pane, view, p, name)

    def click(self, row):
        self.actions["click"] += 1
        self.inject([moment(self.ctx, "PointerButton", button=1, pressed=p,
                            x=row["x"], y=row["y"], space=row["space"]) for p in (True, False)])

    def key(self, chord):
        self.actions["key"] += 1
        self.inject(chord_moments(self.ctx, chord))

    def text(self, text):
        self.actions["text"] += 1
        self.inject(clear_moments(self.ctx) + [moment(self.ctx, "TextEntered", text=text)])

    def drag(self, start, end, duration_ms=900, bend=0, during=None, button=1, held=None, modifiers=0):
        """Press, move, release. Input's timed motion carries no modifier, so a drag with one held
        moves in steps this hand injects itself, each carrying it, in a straight line."""
        self.actions["drag"] += 1
        self.ctx.check(start["space"] == end["space"], "drag endpoints use different spaces")
        self.inject([moment(self.ctx, "PointerButton", button=button, pressed=True, modifiers=modifiers,
                            x=start["x"], y=start["y"], space=start["space"])])
        if modifiers:
            steps = 16
            path = [moment(self.ctx, "PointerMoved", modifiers=modifiers, space=start["space"],
                           x=start["x"] + (end["x"] - start["x"]) * i // steps,
                           y=start["y"] + (end["y"] - start["y"]) * i // steps) for i in range(1, steps + 1)]
            moved = self.inject(path)
            if held is not None:
                held()  # at the endpoint, the button still down
            self.inject([moment(self.ctx, "PointerButton", button=button, pressed=False, modifiers=modifiers,
                                x=end["x"], y=end["y"], space=end["space"])])
            return moved
        fields = {"session": self.session, "x": end["x"], "y": end["y"],
                  "duration_ms": duration_ms, "bend": float(bend)}
        if during is None:
            moved = self.ask("zengine.input", "PointerMotionRequested", fields,
                             settle=True, timeout=duration_ms / 1000 + 10)
        else:
            pending = self.ctx.ask_async("zengine.input", "PointerMotionRequested", fields,
                                         via=self.link, settle=True)
            time.sleep(duration_ms / 2000)  # observation only; Zengine generates all samples
            during()
            moved = pending.wait(duration_ms / 1000 + 10)
        self.ctx.check(moved["session"] == self.session and moved["admitted"] > 0,
                       "pointer motion did not reach its endpoint")
        if held is not None:
            held()  # at the endpoint, the button still down
        self.inject([moment(self.ctx, "PointerButton", button=button, pressed=False,
                            x=end["x"], y=end["y"], space=end["space"])])
        return moved
