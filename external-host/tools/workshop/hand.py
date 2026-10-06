# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Small orchestration helpers. Zengine owns geometry, motion, drops and permissions."""
import json
import time
from collections import Counter
from workshop_steps import moment, chord_moments, clear_moments


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
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    def words(self, provider, pane):
        """A pane's words as Workshop holds them (PaneView version 2): a text pane's rows or a
        canvas pane's labels and runs, each with its place in canvas pixels and the point a press
        names it by."""
        return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane},
                        version=2)

    def word_point(self, provider, pane, word, column, picture):
        """Where one character of one of those words is now; refused if the picture moved."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "word": word, "column": column}, version=2)

    def desk(self):
        """The desk by Workshop's own numbers (DeskView): every pane on it with its state, rank
        from the front, place and size, selection and keys; the room; arranging; the menu."""
        return self.ask("zengine.workshop", "DeskViewRequested", {})

    def row(self, provider, pane, contains, scroll=False):
        # Read the current view, then search toward each edge with ordinary wheel input.
        # A stable view is the edge, not a reason to retry it.
        for direction in ((1, -1) if scroll else (0,)):
            previous = None
            for _ in range(256):
                view = self.view(provider, pane)
                rows = [r for r in view["rows"] if contains in r["text"]]
                if len(rows) == 1:
                    return rows[0]
                self.ctx.check(not rows, "ambiguous visible row: " + contains)
                visible = [(r["row"], r["text"]) for r in view["rows"]]
                if not direction or not visible or visible == previous:
                    break
                previous = visible
                at = view["rows"][0]
                self.actions["wheel"] += 1
                self.inject([moment(self.ctx, "PointerWheel", wheel_dy=direction,
                                    x=at["x"], y=at["y"], space=at["space"])])
        self.ctx.produce("last-view.json", json.dumps({"picture": view["picture"], "rows": view["rows"]}, indent=2).encode())
        raise ValueError("visible row not found: " + contains)

    def point(self, provider, pane, row, column, picture):
        """Where Workshop's own measurer puts one prose cell of a pane now; refused if it moved."""
        return self.ask("zengine.workshop", "PanePointRequested", {"provider": provider, "pane": pane,
                        "picture": picture, "row": row, "column": column})

    def control(self, provider, pane, label):
        """Press a visible `[label]` control. The column comes from the painted row; the screen
        position from Workshop, so no caller multiplies a font metric."""
        view = self.view(provider, pane)
        word = "[" + label + "]"
        for r in view["rows"]:
            at = r["text"].find(word)
            if at >= 0:
                where = self.point(provider, pane, r["row"], at + 1, view["picture"])
                self.click(where)
                return where
        self.ctx.produce("last-view.json", json.dumps({"rows": [x["text"] for x in view["rows"]]}, indent=2).encode())
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
