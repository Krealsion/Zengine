# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Small orchestration helpers. Zengine owns geometry, motion, drops and permissions.

A pane is read by its words (PaneView version 3) wherever a reader finds a row by what it says or
a part by its name -- `row`, `part`, `last_part`, `first`, `control`, `rows_of` -- so a text pane
and a pane that draws a canvas picture read alike: a text pane's word is its row, a canvas pane's
each text run or label it drew, and a row it leaves blank is no word. `view`, `point`, `spot` and
`field` read a text pane's rows by number (version 1), which a canvas pane does not answer."""
import json
import time
from collections import Counter

from loom_session.tool import Refused

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


def rows_of(view):
    """A pane's words in the shape a reader of rows takes: `{picture, canvas, rows}`, each row
    `{row, text, x, y, space}` -- `row` the word's number, which is a text pane's row; `text` its
    characters without the blanks after the last; `x, y, space` the point a press names it by."""
    return {"provider": view["provider"], "pane": view["pane"], "picture": view["picture"],
            "canvas": view.get("canvas", False),
            "rows": [{"row": w["word"], "text": w["text"], "x": w["x"], "y": w["y"],
                      "space": w["space"]} for w in view["words"]]}


class Hand:
    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link
        self.actions = Counter()  # weaver gestures this hand made, by kind
        self.session = self.ask("zengine.input", "InputSessionRequested",
                                {"purpose": "inventory composition demo"})["session"]
        self.open = True
        ctx.on_cleanup(self.close, "release the demo input session")

    def ask(self, role, shape, fields, **kw):
        return self.ctx.ask(role, shape, fields, via=self.link, **kw)

    def close(self):
        if self.open:
            self.ask("zengine.input", "InputSessionClosed", {"session": self.session, "holder": 0})
            self.open = False

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
        """A text pane's rows by number (PaneView version 1), for the readers that address a cell
        by its row and column (`point`); a canvas pane is refused."""
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    def words(self, provider, pane):
        """A pane's words and named parts as Workshop holds them (PaneView version 3): a text pane's
        rows or a canvas pane's labels and runs, each with its place in canvas pixels and the point a
        press names it by, and beside them every part the pane names, under the pane's own name."""
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane},
                        version=3)

    def word_point(self, provider, pane, word, column, picture):
        """Where one character of one of those words is now; refused if the picture moved."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "word": word, "column": column}, version=2)

    def desk(self):
        """The desk by Workshop's own numbers (DeskView version 2): every pane on it with its state,
        rank from the front, place and size, selection and keys; the room; arranging; the menu, and
        the lines it names."""
        return self.ask("zengine.workshop", "DeskViewRequested", {}, version=2)

    def seek(self, provider, pane, find, scroll):
        """Read the pane's words until `find(view)` names something, searching toward each edge
        with ordinary wheel input when `scroll`: one notch at a time at the first word's point,
        until the words stop changing -- a stable view is the edge, not a reason to retry it.
        Returns `(view, found)`, `found` None when no view named anything."""
        view = None
        for direction in ((1, -1) if scroll else (0,)):
            previous = None
            for _ in range(256):
                view = self.words(provider, pane)
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

    def row(self, provider, pane, contains, scroll=False):
        """The one visible row holding `contains` -- a text pane's row, or the one run of text a
        canvas pane draws on a row -- as `{row, text, x, y, space}`, pressed where Workshop says its
        third character is (its own point where it has fewer). With `scroll`, the pane is wheeled
        toward each edge until the row shows. Two rows holding it fail the run's check (the row is
        ambiguous); none raises ValueError and keeps `last-view.json`."""
        def find(view):
            found = [w for w in view["words"] if contains in w["text"]]
            self.ctx.check(len(found) <= 1, "ambiguous visible row: " + contains)
            return found[0] if found else None
        for _ in range(3):
            view, w = self.seek(provider, pane, find, scroll)
            if w is None:
                self.last_view(view)
                raise ValueError("visible row not found: " + contains)
            try:
                at = w if len(w["text"]) < 3 else self.word_point(provider, pane, w["word"], 2,
                                                                    view["picture"])
            except Refused as refused:
                if "picture moved" not in str(refused):
                    raise
                continue  # the pane redrew between the reading and the point: read it again
            return {"row": w["word"], "text": w["text"], "x": at["x"], "y": at["y"],
                    "space": at["space"]}
        raise ValueError("visible row %r kept moving while it was read" % contains)

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
        view = self.words(provider, pane)
        if not view["words"]:
            self.last_view(view)
            raise ValueError("%s/%s draws no word" % (provider, pane))
        w = min(view["words"], key=lambda w: (w["place"]["y"], w["place"]["x"]))
        return {"row": w["word"], "text": w["text"], "x": w["x"], "y": w["y"], "space": w["space"]}

    def point(self, provider, pane, row, column, picture):
        """Where Workshop's own measurer puts one prose cell of a pane now; refused if it moved."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "row": row, "column": column})

    def control(self, provider, pane, label):
        """Press a visible `[label]` control, in a text pane's rows or a canvas pane's words: the
        column comes from the painted word, the screen position from Workshop, so no caller
        multiplies a font metric."""
        view = self.words(provider, pane)
        word = "[" + label + "]"
        for w in view["words"]:
            at = w["text"].find(word)
            if at >= 0:
                where = self.word_point(provider, pane, w["word"], at + 1, view["picture"])
                self.click(where)
                return where
        self.last_view(view)
        raise ValueError("no visible control %s in %s/%s" % (word, provider, pane))

    def spot(self, provider, pane, text, row_prefix=""):
        """Where Workshop paints `text` now, on the first row starting with `row_prefix` that
        holds it -- a location crumb, a control or a folder name -- measured by Workshop."""
        view = self.view(provider, pane)
        for r in view["rows"]:
            if r["text"].startswith(row_prefix) and text in r["text"]:
                return self.point(provider, pane, r["row"], r["text"].find(text) + 1, view["picture"])
        self.ctx.produce("last-view.json", json.dumps({"rows": [x["text"] for x in view["rows"]]}, indent=2).encode())
        raise ValueError("%r is not painted in %s/%s" % (text, provider, pane))

    def field(self, provider, pane, label, notches=64):
        """The row of an Info view field `label:`, walking the view's selection with the wheel
        until it is painted (the window follows the selection)."""
        for direction in (0, -1, 1):
            previous = None
            for _ in range(notches if direction else 1):
                view = self.view(provider, pane)
                for r in view["rows"]:
                    if r["text"][2:].startswith(label + ":"):
                        return r
                visible = [x["text"] for x in view["rows"]]
                if not direction or visible == previous:
                    break
                previous = visible
                at = view["rows"][-1]
                self.actions["wheel"] += 1
                self.inject([moment(self.ctx, "PointerWheel", wheel_dy=direction,
                                    x=at["x"], y=at["y"], space=at["space"])])
        raise ValueError("no field %s in %s/%s" % (label, provider, pane))

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
