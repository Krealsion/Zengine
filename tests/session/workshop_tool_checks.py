# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Entry-point checks for the Workshop Python tools, with a recording transport.

Run by workshop_journey before starting its real processes. These checks pin emitted events,
pre-contact refusal and cleanup, and what `workshop/act` reads from a scripted Workshop; they do
not claim that a real pane consumed those events.
Standalone: python workshop_tool_checks.py --tools <package> --runtime <loom runtime>
"""

import argparse
import importlib
import json
from pathlib import Path
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch


class CheckFailed(Exception):
    pass


class InputOwner:
    def __init__(self):
        self.open = False
        self.closes = 0


class Context:
    name = "tool-check"

    def __init__(self, steps, owner=None, **inputs):
        self.inputs = dict(link="workshop", **inputs)
        self.owner = owner or InputOwner()
        self.steps = steps
        self.contacts = []
        self.cleanups = []
        self.events = []
        self.shape_reads = 0
        self.conn = SimpleNamespace(schema=self.schema)

    def schema(self, *args):
        self.shape_reads += 1
        types = self.steps
        fields = [("kind", types.TEXT), ("scancode", types.INT), ("name", types.TEXT),
                  ("modifiers", types.INT), ("text", types.TEXT), ("button", types.INT),
                  ("pressed", types.BOOL), ("x", types.INT), ("y", types.INT),
                  ("dx", types.INT), ("dy", types.INT), ("space", types.INT),
                  ("wheel_dx", types.FLOAT), ("wheel_dy", types.FLOAT)]
        return SimpleNamespace(fields=[SimpleNamespace(name=n, type=SimpleNamespace(kind=k))
                                       for n, k in fields])

    def check(self, condition, message):
        if not condition:
            raise CheckFailed(message)

    def fail(self, message):
        raise CheckFailed(message)

    def step(self, message):
        pass

    def note(self, message):
        pass

    def produce(self, *args):
        pass

    def on_cleanup(self, callback, label):
        self.cleanups.append(callback)

    def ask(self, office, shape, fields, **options):
        self.contacts.append(shape)
        if shape == "InputSessionRequested":
            assert set(fields) == {"purpose"} and isinstance(fields["purpose"], str)
            assert not self.owner.open, "previous run leaked its input session"
            self.owner.open = True
            return {"session": 1}
        if shape == "InputSessionClosed":
            assert self.owner.open
            self.owner.open = False
            self.owner.closes += 1
            return {}
        if shape == "InjectInput":
            assert self.owner.open and options.get("settle")
            first = len(self.events) + 1
            self.events.extend(fields["events"])
            return {"session": 1, "admitted": len(fields["events"]), "first_seq": first,
                    "last_seq": len(self.events)}
        if shape == "PointerMotionRequested":
            assert self.owner.open and options.get("settle")
            self.events.append({"kind": "PointerMoved", "x": fields["x"], "y": fields["y"]})
            return {"session": 1, "admitted": 1, "first_seq": len(self.events), "last_seq": len(self.events)}
        raise AssertionError(shape)


class Manager:
    """The Pane Manager as a script: its list through a window of `window` rows that follows the
    marker, the keys, and a press that chooses a row -- or, on the marked row with the keys already
    here, opens its pane. Its words stand at x 400 and its parts' points at x 450."""
    canvas = False

    def __init__(self, panes, window=3, shown=False, cursor=0, keys=False):
        self.panes, self.window, self.shown = list(panes), window, shown
        self.cursor, self.first, self.keys, self.open = cursor, 0, keys, set()
        self.selected = None

    def listed(self):
        self.first = min(self.first, self.cursor)
        self.first = max(self.first, self.cursor - self.window + 1)
        return range(self.first, min(len(self.panes), self.first + self.window))

    def rows(self):
        """The rows drawn now, each with the name the pane gives it."""
        out = [("PANES -- %d" % len(self.panes), None)]
        for i in self.listed():
            office, pane, label = self.panes[i]
            mark = "[open]" if (office, pane) in self.open else "[    ]"
            out.append(("%s%s %s" % ("> " if i == self.cursor else "  ", mark, label),
                        "pane:%s/%s" % (office, pane)))
        return out

    def press(self, y):
        self.choose((y - 6) // 12 - 1)

    def choose(self, index):
        """A press on the list's `index`th drawn row (none outside it): it chooses that row -- or,
        marked with the keys here, opens its pane -- and the Pane Manager takes the keys."""
        listed = list(self.listed())
        if 0 <= index < len(listed):
            at = listed[index]
            if at == self.cursor and self.keys:
                self.open.add(self.panes[at][:2])
                self.selected = self.panes[at][:2]
            self.cursor = at
        self.keys = True

    def holds(self, x, y):
        """Whether a press at x, y lands on the Pane Manager."""
        return 400 <= x < 500

    def pressed(self, x, y):
        self.press(y)


class CanvasPane:
    """A pane that draws its rows on its canvas, as Workshop reads one (PaneView version 3, canvas
    true): one unpadded run a row on the lattice -- `inset` px in from the body's left, `advance`
    by `line` px a character, rows from the body's top -- each run's word its characters without
    the blanks after the last, so a blank row is no word and a word's number is not its row's; each
    named part the rectangle over its row's columns, its text the characters inside it, its point
    the middle of its widest stretch of columns no later part takes (none, in no space, where every
    column is another's). `drop` stands a word's place that many px below its row's top, as a run
    set in from its row's rectangle; `by_column` lists every row's first run before any row's
    second, as Workshop lists a canvas pane's labels before its runs. A press or a wheel inside the
    body is the pane's, read back to a row and a column."""
    canvas = True

    def __init__(self, body=(600, 100, 420, 168), inset=2, advance=7, line=14, drop=0,
                 by_column=False):
        self.body, self.inset, self.advance, self.line = body, inset, advance, line
        self.drop, self.by_column, self.said = drop, by_column, []

    def columns(self):
        return (self.body[2] - 2 * self.inset) // self.advance

    def column_x(self, column):
        return self.body[0] + self.inset + column * self.advance

    def row_y(self, row):
        return self.body[1] + row * self.line

    def drawn(self):
        """Each row drawn now: `{"runs": [(column, text)], "parts": [(name, column, columns)]}`."""
        raise NotImplementedError

    def view(self, provider, pane, pointed=lambda part: part):
        rows = self.drawn()
        runs = [(r, i, c, t.rstrip(" ")) for r, row in enumerate(rows)
                for i, (c, t) in enumerate(row["runs"]) if t.rstrip(" ")]
        if self.by_column:
            runs.sort(key=lambda run: (run[1], run[0]))
        self.said = [(r, c, t) for r, _, c, t in runs]
        words = [{"word": n, "text": t,
                  "place": {"x": self.column_x(c), "y": self.row_y(r) + self.drop,
                            "w": len(t) * self.advance, "h": self.line - self.drop},
                  "x": self.column_x(c + (len(t) - 1) // 2) + self.advance // 2,
                  "y": self.row_y(r) + self.line // 2, "space": 2}
                 for n, (r, c, t) in enumerate(self.said)]
        listed = [(r, name, c, n) for r, row in enumerate(rows) for name, c, n in row["parts"]]
        owner = {}
        for i, (r, _, c, n) in enumerate(listed):
            for column in range(c, c + n):
                owner[(r, column)] = i
        parts = []
        for i, (r, name, c, n) in enumerate(listed):
            line = "".ljust(self.columns())
            for column, text in rows[r]["runs"]:
                line = line[:column] + text + line[column + len(text):]
            own = [column for column in range(c, c + n) if owner[(r, column)] == i]
            stretches = []
            for column in own:
                if stretches and stretches[-1][-1] == column - 1:
                    stretches[-1].append(column)
                else:
                    stretches.append([column])
            part = {"name": name, "text": line[c:c + n].rstrip(" "),
                    "place": {"x": self.column_x(c), "y": self.row_y(r), "w": n * self.advance,
                              "h": self.line}, "x": 0, "y": 0, "space": 0}
            if stretches:
                widest = max(stretches, key=len)
                part.update(x=self.column_x(widest[0]) + len(widest) * self.advance // 2,
                            y=self.row_y(r) + self.line // 2, space=2)
            parts.append(pointed(part))
        return {"provider": provider, "pane": pane, "picture": 1, "canvas": True,
                "words": words, "parts": parts}

    def point(self, word, column):
        """Where one character of a word last said is (PanePoint version 2)."""
        from loom_session.tool import Refused
        if word >= len(self.said) or column >= len(self.said[word][2]):
            raise Refused("pane point unavailable: outside the pane's visible words")
        r, c, _ = self.said[word]
        return {"x": self.column_x(c + column) + self.advance // 2,
                "y": self.row_y(r) + self.line // 2, "space": 2}

    def at(self, x, y):
        """The row and column a point inside the body stands on, or None outside it."""
        bx, by, bw, bh = self.body
        if not (bx <= x < bx + bw and by <= y < by + bh):
            return None
        return (y - by) // self.line, (x - bx - self.inset) // self.advance

    def holds(self, x, y):
        return self.at(x, y) is not None

    def pressed(self, x, y):
        pass

    def wheeled(self, dy):
        pass


class CanvasManager(Manager, CanvasPane):
    """The Pane Manager drawing its rows on its canvas: its heading on the first row, a blank row,
    then its list -- each row one run, or with `split` two, its marker and mark and then its label
    after a blank -- each with `pane:<office>/<pane>` over the row and `mark:<office>/<pane>` over
    its mark, listed after it."""
    canvas = True

    def __init__(self, panes, split=False, drop=0, **kw):
        Manager.__init__(self, panes, **kw)
        CanvasPane.__init__(self, drop=drop, by_column=split)
        self.split = split

    def drawn(self):
        out = [{"runs": [(0, "PANES -- %d" % len(self.panes))], "parts": []},
               {"runs": [], "parts": []}]
        for i in self.listed():
            office, pane, label = self.panes[i]
            head = ("> " if i == self.cursor else "  ") + (
                "[open]" if (office, pane) in self.open else "[    ]")
            ref = "%s/%s" % (office, pane)
            out.append({"runs": [(0, head), (9, label)] if self.split else [(0, head + " " + label)],
                        "parts": [("pane:" + ref, 0, self.columns()), ("mark:" + ref, 2, 6)]})
        return out

    def holds(self, x, y):
        return CanvasPane.holds(self, x, y)

    def pressed(self, x, y):
        self.choose(self.at(x, y)[0] - 2)


class CanvasLoaded(CanvasPane):
    """Loaded drawing its rows on its canvas: a heading, a blank row, a window of `window` weaves
    from `origin` -- each row `weave:<name>` -- and what the window leaves out. A wheel notch away
    from the weaver moves the window one weave toward the first, toward the weaver one toward the
    last; a press on a weave's row chooses it."""

    def __init__(self, weaves, window=3, origin=0, **kw):
        CanvasPane.__init__(self, **kw)
        self.weaves, self.window, self.origin, self.chosen = list(weaves), window, origin, []

    def drawn(self):
        shown = self.weaves[self.origin:self.origin + self.window]
        out = [{"runs": [(0, "loaded weaves -- %d" % len(self.weaves))], "parts": []},
               {"runs": [], "parts": []}]
        out += [{"runs": [(0, "  %s @%s" % w)], "parts": [("weave:" + w[0], 0, self.columns())]}
                for w in shown]
        out.append({"runs": [(0, "  ... %d earlier, %d more" % (
            self.origin, len(self.weaves) - self.origin - len(shown)))], "parts": []})
        return out

    def wheeled(self, dy):
        self.origin = max(0, min(len(self.weaves) - self.window, self.origin - int(dy)))

    def pressed(self, x, y):
        row = self.at(x, y)[0] - 2
        if 0 <= row < self.window:
            self.chosen.append(self.weaves[self.origin + row][0])


class CanvasControls(CanvasPane):
    """The demo's controls drawing their rows on their canvas: the demo's name, `[ Reset demo ]`
    named `control:reset`, and its state named `status`; a press on the reset row resets."""

    def __init__(self, **kw):
        CanvasPane.__init__(self, body=(20, 500, 392, 56), **kw)
        self.resets = 0

    def drawn(self):
        return [{"runs": [(0, "Demo")], "parts": []},
                {"runs": [(0, "[ Reset demo ]")], "parts": [("control:reset", 0, self.columns())]},
                {"runs": [(0, "ready: Ready")], "parts": [("status", 0, self.columns())]}]

    def pressed(self, x, y):
        if self.at(x, y)[0] == 1:
            self.resets += 1


class Answer(dict):
    """An owner's answer as a run reads it: by its fields' names, and whole as `fields`."""

    @property
    def fields(self):
        return dict(self)


class CanvasWorkshop(Context):
    """A run context whose Workshop shows `panes` -- {(provider, pane): CanvasPane} -- that draw
    pictures: it answers PaneView version 3 and PanePoint version 2, and fails a run that asks any
    other version of either, as a pane drawing a picture is not read by its rows. A part `unreached`
    names has no point. Each press or wheel inside a pane's body is that pane's. `zengine.demo`
    answers its status by the resets its controls were pressed for. `act_steps` are a
    `workshop/act` run's steps; what a run keeps is kept."""

    def __init__(self, steps, panes, act_steps=(), unreached=()):
        Context.__init__(self, steps)
        self.inputs["steps"] = json.dumps(list(act_steps))
        self.panes, self.unreached, self.asked, self.kept = dict(panes), set(unreached), [], {}

    def produce(self, name, data):
        self.kept[name] = data

    def pointed(self, part):
        if part["name"] in self.unreached:
            part.update(x=0, y=0, space=0)
        return part

    def ask(self, office, shape, fields, **options):
        if shape in ("PaneViewRequested", "PanePointRequested"):
            version = options.get("version", 1)
            self.asked.append((shape, version))
            if version != (3 if shape == "PaneViewRequested" else 2):
                raise AssertionError("%s version %d asked of a pane that draws a picture"
                                     % (shape, version))
            pane = self.panes[(fields["provider"], fields["pane"])]
            if shape == "PanePointRequested":
                return pane.point(fields["word"], fields["column"])
            return pane.view(fields["provider"], fields["pane"], self.pointed)
        if shape == "InjectInput":
            for e in fields["events"]:
                for pane in self.panes.values():
                    if e["kind"] in ("PointerButton", "PointerWheel") and pane.holds(e["x"], e["y"]):
                        if e["kind"] == "PointerWheel":
                            pane.wheeled(e["wheel_dy"])
                        elif e["pressed"]:
                            pane.pressed(e["x"], e["y"])
        if shape == "DemoStatusRequested":
            return Answer(generation=3)
        if shape == "DemoReadyRequested":
            controls = self.panes[("zengine.demo", "controls")]
            ready = controls.resets == 1 and fields["generation"] == 4
            return Answer(state="ready" if ready else "failed", generation=fields["generation"],
                          note="Ready" if ready else "no reset was pressed")
        return Context.ask(self, office, shape, fields, **options)


class ActWorkshop(Context):
    """A run context whose Workshop is a script for `workshop/act`: Info's list with a cursor the
    Down and Up keys move, a canvas pane's words, any other pane's rows as `panes` names them and
    the names `named` gives their rows, a point door that says which word and column it was asked
    for and refuses a character no word shows, a Pane Manager when `manager` is one -- a `Manager`
    as text rows, or a `CanvasManager` drawing a picture -- and a desk with a menu open whose lines
    are named. A part or a line `unreached` names has no point, as Workshop says one no press
    reaches on its own. Every injected moment is kept, as `Context` keeps them."""

    def __init__(self, steps, act_steps, rows, panes=None, named=None, manager=None, unreached=()):
        Context.__init__(self, steps)
        self.inputs["steps"] = json.dumps(act_steps)  # the input shares the module's name
        self.base = list(rows)
        self.cursor = next((i for i, r in enumerate(rows) if r.startswith(">")), 0)
        self.panes, self.named, self.said = dict(panes or {}), dict(named or {}), {}
        self.manager, self.unreached = manager, set(unreached)
        self.points, self.kept = [], {}

    def rows(self):
        return [(">" if i == self.cursor else " ") + r[1:] for i, r in enumerate(self.base)]

    def produce(self, name, data):
        self.kept[name] = data

    def done(self):
        return json.loads(self.kept["steps.json"])

    def pointed(self, part):
        """A named part as Workshop says it: with no point -- 0, 0 in no space -- where `unreached`
        names it."""
        if part["name"] in self.unreached:
            part.update(x=0, y=0, space=0)
        return part

    def moved(self, e):
        """What one injected moment does to the script: Ctrl+P shows or hides the Pane Manager,
        Down and Up walk the cursor of whichever list holds the keys, a press on the Pane Manager's
        words is its."""
        m = self.manager
        if e["kind"] == "KeyPressed" and e["scancode"] == 19 and e["modifiers"] and m:
            m.shown = not m.shown
            m.keys = m.shown
        elif e["kind"] == "KeyPressed" and e["scancode"] in (81, 82):
            step = 1 if e["scancode"] == 81 else -1
            if m and m.shown and m.keys:
                m.cursor = max(0, min(len(m.panes) - 1, m.cursor + step))
            else:
                self.cursor = max(0, min(len(self.base) - 1, self.cursor + step))
        elif e["kind"] == "PointerButton" and e["pressed"] and m and m.shown:
            if m.holds(e["x"], e["y"]):
                m.pressed(e["x"], e["y"])
            else:
                m.keys = False

    def ask(self, office, shape, fields, **options):
        from loom_session.tool import Refused
        if shape == "InjectInput":
            for e in fields["events"]:
                self.moved(e)
        if shape == "PaneViewRequested" and options.get("version") == 3:
            if fields["pane"] == "launcher":
                if not (self.manager and self.manager.shown):
                    raise Refused("pane view unavailable: closed, unknown or covered by an interaction")
                if self.manager.canvas:
                    return self.manager.view(fields["provider"], fields["pane"], self.pointed)
                drawn = self.manager.rows()
                texts, names, left = [t for t, _ in drawn], [n for _, n in drawn], 400
            else:
                canvas = fields["pane"] == "view-builder"
                texts = self.panes.get(fields["pane"]) or (["node one", "[Label]"] if canvas
                                                            else self.rows())
                names, left = self.named.get(fields["pane"], []), 0
            self.said[fields["pane"]] = texts
            return {"provider": fields["provider"], "pane": fields["pane"], "picture": 1,
                    "canvas": fields["pane"] == "view-builder", "words": [
                        {"word": i, "text": t, "place": {"x": left, "y": 12 * i, "w": 12 * len(t), "h": 12},
                         "x": left + 6, "y": 12 * i + 6, "space": 2} for i, t in enumerate(texts)],
                    "parts": [self.pointed(
                        {"name": n, "text": texts[i], "place": {"x": left, "y": 12 * i, "w": 480, "h": 12},
                         "x": left + 50, "y": 12 * i + 6, "space": 2})
                        for i, n in enumerate(names) if n and i < len(texts)]}
        if shape == "PanePointRequested" and options.get("version") == 2:
            if fields["pane"] == "launcher" and self.manager.canvas:
                return self.manager.point(fields["word"], fields["column"])
            said = self.said.get(fields["pane"], [])
            if fields["word"] >= len(said) or fields["column"] >= len(said[fields["word"]]):
                raise Refused("pane point unavailable: outside the pane's visible words")
            self.points.append((fields["word"], fields["column"]))
            if fields["pane"] == "launcher":
                return {"x": 400 + fields["column"], "y": 12 * fields["word"] + 6, "space": 2}
            return {"x": 100 + fields["word"], "y": fields["column"], "space": 2}
        if shape == "DeskViewRequested" and options.get("version") == 2:
            place = {"x": 0, "y": 24, "w": 480, "h": 300}
            panes = [{"provider": "zengine.info", "pane": "info", "name": "Info",
                      "state": "open", "front": 0, "selected": True, "keys": True,
                      "visible": place, "resolved": place}]
            m = self.manager
            if m:
                panes[0]["keys"] = False
                panes.append({"provider": "zengine.desktop", "pane": "launcher", "name": "Pane Manager",
                              "state": "open" if m.shown else "closed", "front": 1, "selected": m.keys,
                              "keys": m.keys, "visible": place, "resolved": place})
                for office, pane, label in m.panes:
                    if (office, pane) != ("zengine.info", "info"):
                        panes.append({"provider": office, "pane": pane, "name": label,
                                      "state": "covered" if (office, pane) in m.open else "closed",
                                      "front": -1, "selected": (office, pane) == m.selected,
                                      "keys": False, "visible": {}, "resolved": {}})
            return {"width": 1440, "height": 900, "cell_px": 12, "space": 2,
                    "room": {"x": 0, "y": 24, "w": 1440, "h": 800}, "arranging": False,
                    "panes": panes,
                    "menu": {"open": True, "office": "zengine.files", "pane": "files", "picture": 3,
                             "place": {"x": 20, "y": 30, "w": 100, "h": 40},
                             "lines": [{"word": 0, "text": "> Rename", "x": 30, "y": 41, "space": 2},
                                       {"word": 1, "text": "  Delete", "x": 30, "y": 59, "space": 2}],
                             "parts": [self.pointed(
                                           {"name": "file.rename", "text": "> Rename", "x": 31, "y": 41,
                                            "space": 2, "place": {"x": 20, "y": 35, "w": 100, "h": 12}}),
                                       self.pointed(
                                           {"name": "file.delete", "text": "  Delete", "x": 31, "y": 59,
                                            "space": 2, "place": {"x": 20, "y": 53, "w": 100, "h": 12}})]}}
        return Context.ask(self, office, shape, fields, **options)


def link_status(ctx, link):
    ctx.contacts.append("link status")
    return {"session": 2, "established_name": "test"}


def inventory(ctx, link, status):
    ctx.contacts.append("inventory")
    return {"rows": [1]}, {"established": "test"}


def picture(ctx, link, name):
    ctx.contacts.append("picture " + name)
    return {"frame": 1 if name == "before" else 2, "format": "text/cells",
            "width": 80, "height": 24}, name.encode()


def run_checks(tools, runtime):
    sys.path[:0] = [str(Path(runtime).resolve()), str(Path(tools).resolve())]
    capture = importlib.import_module("inspect_capture")
    steps = importlib.import_module("workshop_steps")
    recipe = importlib.import_module("verify_recipe")
    capture_inventory = importlib.import_module("inventory_capture")
    collect = importlib.import_module("inventory_collect")
    drag = importlib.import_module("drag")
    act = importlib.import_module("act")
    hand = importlib.import_module("hand")
    reset_button = importlib.import_module("demo_reset_button")

    class ToolChecks(unittest.TestCase):
        def test_drag_delegates_timed_motion_and_cleans_up_after_picture_failure(self):
            for fail in (False, True):
                ctx = Context(steps, start="2,3@console", end="20,10@console")
                def capture_picture(context, link, name):
                    if fail and name == "after":
                        raise RuntimeError("picture failed")
                    return picture(context, link, name)
                with patch.multiple(drag, link_session=link_status, picture=capture_picture):
                    try:
                        if fail:
                            with self.assertRaisesRegex(RuntimeError, "picture failed"):
                                drag.run(ctx)
                        else:
                            self.assertIn("timed drag", drag.run(ctx))
                    finally:
                        for cleanup in reversed(ctx.cleanups): cleanup()
                self.assertEqual([e["kind"] for e in ctx.events],
                                 ["PointerButton", "PointerMoved", "PointerButton"])
                self.assertEqual((ctx.events[0]["x"], ctx.events[-1]["x"]), (2, 20))
                self.assertFalse(ctx.owner.open)
                self.assertEqual(ctx.owner.closes, 1)

        def test_drag_rejects_mixed_coordinate_spaces_before_contact(self):
            ctx = Context(steps, start="2,3@console", end="20,10")
            with self.assertRaisesRegex(ValueError, "same coordinate space"):
                drag.run(ctx)
            self.assertEqual(ctx.contacts, [])

        def test_collection_readback_checks_reference_revision_and_bytes(self):
            entry = {"reference": {"owner": "owner", "entry": "saved"}, "revision": 1, "pair": b"capture"}
            for field, changed in (("reference", {"owner": "owner", "entry": "other"}),
                                   ("revision", 2), ("pair", b"changed")):
                ctx = Context(steps, target_role="target", label="saved")
                produced = {}
                ctx.produce = lambda name, value: produced.update({name: value})
                later = dict(entry, **{field: changed})
                ctx.ask = lambda office, shape, fields, **kw: entry if shape == "InventoryCaptureAdd" else later
                with patch.object(collect, "link_session", link_status):
                    with self.assertRaisesRegex(CheckFailed, "captured entry changed"):
                        collect.run(ctx)
                self.assertEqual(produced["pair.bin"], b"capture")
                self.assertEqual(json.loads(produced["entry.json"])["reference"], entry["reference"])

        def act(self, ctx):
            try:
                return act.run(ctx)
            finally:
                for callback in reversed(ctx.cleanups):
                    callback()

        def execute(self, ctx):
            with patch.multiple(capture, link_session=link_status, own_row=inventory,
                                picture=picture):
                try:
                    return capture.run(ctx)
                finally:
                    # The runner invokes registered cleanup even after an entry point throws.
                    for callback in reversed(ctx.cleanups):
                        callback()

        def test_inventory_readback_cannot_substitute_another_capture(self):
            for current in (b"own capture", b"later writer"):
                ctx = Context(steps, target_role="test.target")
                produced = {}
                ctx.produce = lambda name, value: produced.update({name: value})
                ctx.ask = lambda office, shape, fields, **kw: (
                    {"pair": b"own capture"} if shape == "InventoryCaptureDescribe"
                    else {"occupied": True, "pair": current})
                with patch.object(capture_inventory, "link_session", link_status):
                    if current == b"own capture":
                        self.assertIn("confirmed", capture_inventory.run(ctx))
                    else:
                        with self.assertRaisesRegex(CheckFailed, "changed after capture"):
                            capture_inventory.run(ctx)
                self.assertEqual(produced, {"pair.bin": b"own capture"})

        def test_click_is_only_pointer_press_and_release(self):
            ctx = Context(steps, click="2,21@console", chord="", button="right")
            self.execute(ctx)
            self.assertEqual([(e["kind"], e["pressed"], e["button"])
                              for e in ctx.events],
                             [("PointerButton", True, 3), ("PointerButton", False, 3)])
            self.assertEqual(ctx.owner.closes, 1)

        def test_replacement_erases_before_optional_text_and_commit(self):
            for text in ("", "replacement"):
                with self.subTest(text=text):
                    ctx = Context(steps, chord="enter", clear=True, text=text)
                    self.execute(ctx)
                    keys = [(e["kind"], e["scancode"], e["modifiers"])
                            for e in ctx.events if e["kind"] != "TextEntered"]
                    self.assertEqual(keys, [("KeyPressed", 4, 2), ("KeyReleased", 4, 2),
                                            ("KeyPressed", 42, 0), ("KeyReleased", 42, 0),
                                            ("KeyPressed", 40, 0), ("KeyReleased", 40, 0)])
                    self.assertEqual(len(ctx.events), 7 if text else 6)
                    if text:
                        self.assertEqual(ctx.events[4]["kind"], "TextEntered")
                        self.assertEqual(ctx.events[4]["text"], text)

        def test_boundary_is_checked_before_contact_or_event_construction(self):
            for inputs in (dict(chord="down", repeat=32),
                           dict(chord="down", repeat=31, click="2,21@console")):
                with self.subTest(inputs=inputs):
                    ctx = Context(steps, **inputs)
                    self.execute(ctx)
                    self.assertEqual(len(ctx.events), 64)
            for inputs in (dict(chord="down", repeat=33),
                           dict(chord="down", repeat=32, text="x"),
                           dict(chord="down", repeat=10**12)):
                with self.subTest(inputs=inputs):
                    ctx = Context(steps, **inputs)
                    with patch.object(capture, "chord_moments",
                                      side_effect=AssertionError("constructed before refusal")):
                        with self.assertRaisesRegex(CheckFailed, "64-moment ceiling"):
                            self.execute(ctx)
                    self.assertEqual(ctx.contacts, [])
                    self.assertEqual(ctx.shape_reads, 0)

        def test_failure_after_open_cleans_up_and_next_run_reuses_owner(self):
            owner = InputOwner()
            bad = Context(steps, owner, chord="enter", bug_after="open")
            with self.assertRaisesRegex(RuntimeError, "deliberate tool bug"):
                self.execute(bad)
            self.assertFalse(owner.open)
            self.assertEqual(owner.closes, 1)
            good = Context(steps, owner, chord="down")
            self.execute(good)
            self.assertFalse(owner.open)
            self.assertEqual(owner.closes, 2)
            self.assertEqual(len(good.events), 2)

        def test_asserted_field_absence_and_wrong_types_are_not_blank(self):
            with tempfile.TemporaryDirectory(prefix="zengine-recipe-check-") as temp:
                target = Path(temp) / "build-recipes.json"
                for field in ("artifact_dir", "config"):
                    for state in ("absent", "null", "number", "list", "blank", "value"):
                        with self.subTest(field=field, state=state):
                            row = {"recipe": "test", "cmake_target": [{}]}
                            container = row if field == "artifact_dir" else row["cmake_target"][0]
                            if state != "absent":
                                container[field] = {"null": None, "number": 0, "list": [],
                                                    "blank": "", "value": "Debug"}[state]
                            target.write_text(json.dumps({"fields": {
                                "format": "zengine-build-recipes", "recipes": [row]}}),
                                encoding="utf-8")
                            assertion = "artifact_dir_blank" if field == "artifact_dir" else "cmake_config_blank"
                            ctx = Context(steps, project=temp, recipe="test", **{assertion: True})
                            if state == "blank":
                                self.assertIn("matches every field", recipe.run(ctx))
                            else:
                                with self.assertRaisesRegex(CheckFailed, "does not match"):
                                    recipe.run(ctx)

        # ---- workshop/act's readings, against a scripted Workshop -------------------------------
        def test_a_cell_point_says_which_cells_and_a_bare_one_is_refused(self):
            self.assertEqual(steps.point("126,42"), (126, 42, steps.SPACE_PIXELS))
            self.assertEqual(steps.point("2,3@console"), (2, 3, steps.SPACE_CELLS))
            self.assertEqual(steps.point("2,3@canvas"), (2, 3 + steps.CANVAS_TOP_ROW, steps.SPACE_CELLS))
            for bad in ("2,3c", "2,3@screen", "2,3@", "2@canvas"):
                with self.subTest(bad=bad):
                    with self.assertRaisesRegex(ValueError, "@console"):
                        steps.point(bad)

        def test_select_reads_a_one_column_marker_as_it_reads_a_two_column_one(self):
            for rows, name, presses in (
                    ([">Width       480", " Height      300", " Placement"], "Height", 1),
                    (["> Layouts", "  Hello", "  Files"], "Files", 2),
                    ([" Width       480", " Height      300", ">Placement"], "Width", 2),
                    (["> [open] Layouts", "  [    ] Layouts"], "[    ] Layouts", 1)):
                with self.subTest(name=name):
                    ctx = ActWorkshop(steps, [{"select": ["zengine.info", "info", name]}], rows)
                    self.act(ctx)
                    self.assertEqual(ctx.done()[0]["presses"], presses)
                    self.assertTrue(ctx.rows()[ctx.cursor].startswith(">"))
                    self.assertTrue(act.row_names(ctx.rows()[ctx.cursor], name))
            ctx = ActWorkshop(steps, [{"select": ["zengine.info", "info", "Depth"]}],
                              [">Width       480", " Height      300"])
            with self.assertRaisesRegex(CheckFailed, "marks no row named 'Depth'"):
                self.act(ctx)

        def test_desk_checks_a_place_by_its_number_and_fails_with_what_it_said(self):
            ctx = ActWorkshop(steps, [{"desk": ["zengine.info", "info"],
                                       "is": {"state": "open", "keys": True,
                                              "visible": {"w": 480, "h": 300}}}], [])
            self.act(ctx)
            self.assertEqual(ctx.done()[0]["said"]["visible"]["w"], 480)
            ctx = ActWorkshop(steps, [{"desk": ["zengine.info", "info"], "is": {"visible": {"w": 481}},
                                       "seconds": 0}], [])
            with self.assertRaisesRegex(CheckFailed, r'never held \{"visible": \{"w": 481\}\}'):
                self.act(ctx)
            self.assertIn("failed-step-desk.json", ctx.kept)
            ctx = ActWorkshop(steps, [{"desk": [], "is": {"arranging": False, "menu": True}}], [])
            self.act(ctx)
            ctx = ActWorkshop(steps, [{"desk": ["zengine.info", "info"], "is": {"wide": 1}}], [])
            with self.assertRaisesRegex(CheckFailed, "wide is not one of"):
                self.act(ctx)

        def test_menu_presses_the_line_holding_its_text_at_the_point_workshop_gave(self):
            ctx = ActWorkshop(steps, [{"menu": "Rename"}], [])
            self.act(ctx)
            presses = [e for e in ctx.events if e["kind"] == "PointerButton"]
            self.assertEqual([(e["x"], e["y"], e["space"]) for e in presses], [(30, 41, 2)] * 2)
            ctx = ActWorkshop(steps, [{"menu": "e", "seconds": 0}], [])
            with self.assertRaisesRegex(CheckFailed, "2 lines hold 'e'"):
                self.act(ctx)
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])

        def test_into_gives_a_pane_the_keys_whatever_its_first_row_holds(self):
            # A blank first row has no character to name: it is pressed at the point its word gives.
            for first, where in (("", (6, 6)), ("notes", (100, 0))):
                with self.subTest(first=first):
                    ctx = ActWorkshop(steps, [{"into": ["zengine.editor", "editor"]}], [],
                                      panes={"editor": [first, "press [Save] here"]})
                    self.act(ctx)
                    presses = [e for e in ctx.events if e["kind"] == "PointerButton"]
                    self.assertEqual([(e["x"], e["y"], e["pressed"]) for e in presses],
                                     [where + (True,), where + (False,)])
                    self.assertEqual(ctx.points, [] if not first else [(0, 0)])

        def test_click_and_control_press_a_canvas_word_by_what_it_says(self):
            for step, column in (({"click": ["zengine.view.builder", "view-builder", "Label"]}, 1),
                                 ({"control": ["zengine.view.builder", "view-builder", "Label"]}, 1),
                                 ({"click": ["zengine.view.builder", "view-builder", "node"]}, 0)):
                with self.subTest(step=step):
                    ctx = ActWorkshop(steps, [step], [])
                    self.act(ctx)
                    word = 1 if column == 1 else 0
                    self.assertEqual(ctx.points, [(word, column)])
                    presses = [e for e in ctx.events if e["kind"] == "PointerButton"]
                    self.assertEqual([(e["x"], e["y"]) for e in presses], [(100 + word, column)] * 2)

        INFO_NAMES = {"info": ["property:Width", "property:Height", "property:Placement"]}
        INFO_ROWS = [">Width       480", " Height      300", " Placement"]
        LISTED = [("zengine.info", "info", "Info"), ("zengine.desktop", "hotkeys", "Hotkeys"),
                  ("zengine.terminal", "terminal", "Terminal"), ("zengine.editor", "editor", "Editor"),
                  ("zengine.files", "project-files", "Files"), ("zengine.builder-pane", "builder", "Builder")]

        def test_part_presses_a_named_part_at_its_point_and_fails_with_the_names_drawn(self):
            ctx = ActWorkshop(steps, [{"part": ["zengine.info", "info", "property:Height"]}],
                              self.INFO_ROWS, named=self.INFO_NAMES)
            self.act(ctx)
            presses = [e for e in ctx.events if e["kind"] == "PointerButton"]
            self.assertEqual([(e["x"], e["y"], e["pressed"]) for e in presses],
                             [(50, 18, True), (50, 18, False)])
            self.assertEqual(ctx.done()[0]["text"], " Height      300")
            ctx = ActWorkshop(steps, [{"part": ["zengine.info", "info", "property:Depth"], "seconds": 0}],
                              self.INFO_ROWS, named=self.INFO_NAMES)
            with self.assertRaisesRegex(CheckFailed, "draws no part named 'property:Depth'"):
                self.act(ctx)
            self.assertEqual(json.loads(ctx.kept["failed-step-parts.json"]), self.INFO_NAMES["info"])
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])

        def test_a_part_workshop_gives_no_point_is_never_pressed_and_the_step_says_why(self):
            ctx = ActWorkshop(steps, [{"part": ["zengine.info", "info", "property:Height"]}],
                              self.INFO_ROWS, named=self.INFO_NAMES, unreached={"property:Height"})
            with self.assertRaisesRegex(CheckFailed, "no press reaches 'property:Height' in "
                                        "zengine.info/info on its own"):
                self.act(ctx)
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])
            ctx = ActWorkshop(steps, [{"menu": "file.delete"}], [], unreached={"file.delete"})
            with self.assertRaisesRegex(CheckFailed, "no press reaches the line named 'file.delete'"):
                self.act(ctx)
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])
            manager = Manager(self.LISTED, shown=True)
            ctx = ActWorkshop(steps, [{"open": "pane:zengine.terminal/terminal"}], [], manager=manager,
                              unreached={"pane:zengine.terminal/terminal"})
            with self.assertRaisesRegex(CheckFailed, "no press reaches the Pane Manager's row "
                                        "'pane:zengine.terminal/terminal'"):
                self.act(ctx)
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])
            self.assertEqual(manager.open, set())

        def test_select_chooses_a_row_by_its_name_before_its_text(self):
            for name, presses, at in (("property:Placement", 2, 2), ("property:Width", 0, 0)):
                with self.subTest(name=name):
                    ctx = ActWorkshop(steps, [{"select": ["zengine.info", "info", name]}],
                                      self.INFO_ROWS, named=self.INFO_NAMES)
                    self.act(ctx)
                    self.assertEqual(ctx.done()[0]["presses"], presses)
                    self.assertEqual(ctx.cursor, at)

        def test_menu_presses_a_line_by_its_name_at_the_point_workshop_gave(self):
            ctx = ActWorkshop(steps, [{"menu": "file.delete"}], [])
            self.act(ctx)
            presses = [e for e in ctx.events if e["kind"] == "PointerButton"]
            self.assertEqual([(e["x"], e["y"]) for e in presses], [(31, 59)] * 2)
            self.assertEqual(ctx.done()[0]["name"], "file.delete")

        def test_open_presses_the_pane_managers_row_by_its_name_until_the_desk_says_it_is_open(self):
            # HIDDEN, AND THE ROW OUT OF THE WINDOW: Ctrl+P, a press on the heading for the keys,
            # Down until the window draws the row -- marked, since the window follows the marker --
            # and one press opening it.
            manager = Manager(self.LISTED)
            ctx = ActWorkshop(steps, [{"open": "pane:zengine.files/project-files"}], [], manager=manager)
            self.act(ctx)
            record = ctx.done()[0]
            self.assertEqual((record["opened"], record["presses"], record["state"]),
                             ("pane:zengine.files/project-files", 1, "covered"))
            self.assertEqual(manager.open, {("zengine.files", "project-files")})
            presses = [(e["x"], e["y"]) for e in ctx.events if e["kind"] == "PointerButton" and e["pressed"]]
            self.assertEqual(presses, [(400, 6), (450, 42)])
            self.assertEqual([e["scancode"] for e in ctx.events if e["kind"] == "KeyPressed"],
                             [19, 81, 81, 81, 81])
            # DRAWN AND NOT MARKED: a press choosing it, then, once it shows marked, one opening it.
            manager = Manager(self.LISTED, shown=True)
            ctx = ActWorkshop(steps, [{"open": "pane:zengine.terminal/terminal"}], [], manager=manager)
            self.act(ctx)
            self.assertEqual(ctx.done()[0]["presses"], 2)
            presses = [(e["x"], e["y"]) for e in ctx.events if e["kind"] == "PointerButton" and e["pressed"]]
            self.assertEqual(presses, [(450, 42)] * 2)
            self.assertEqual(manager.open, {("zengine.terminal", "terminal")})
            # ALREADY MARKED, WITH THE KEYS THERE: one press opens it.
            manager = Manager(self.LISTED, shown=True, cursor=1, keys=True)
            ctx = ActWorkshop(steps, [{"open": "pane:zengine.desktop/hotkeys"}], [], manager=manager)
            self.act(ctx)
            self.assertEqual(ctx.done()[0]["presses"], 1)
            self.assertEqual(manager.open, {("zengine.desktop", "hotkeys")})

        def test_open_fails_with_the_names_listed_and_refuses_a_name_that_is_not_a_rows(self):
            manager = Manager(self.LISTED, shown=True)
            ctx = ActWorkshop(steps, [{"open": "pane:zengine.flow/flow"}], [], manager=manager)
            with self.assertRaisesRegex(CheckFailed, "names no row 'pane:zengine.flow/flow'"):
                self.act(ctx)
            self.assertEqual(json.loads(ctx.kept["failed-step-parts.json"]),
                             sorted("pane:%s/%s" % (o, p) for o, p, _ in self.LISTED))
            self.assertEqual(manager.open, set())
            for name in ("Files", "pane:zengine.files", "pane:/files", 7):
                with self.subTest(name=name):
                    ctx = ActWorkshop(steps, [{"open": name}], [], manager=Manager(self.LISTED))
                    with self.assertRaisesRegex(ValueError, "pane:<office>/<pane>"):
                        self.act(ctx)
                    self.assertEqual(ctx.contacts, [])

        # ---- panes that draw a picture, read by their words and their names --------------------
        # Each stand-in below answers only PaneView version 3 and PanePoint version 2 for such a
        # pane: a reading of rows by number fails the check that made it.
        LOADED = ("zengine.introspection", "loaded")
        WEAVES = [("zengine-input", "zengine.input"), ("zengine-skin-sdl", "zengine.skin"),
                  ("zengine-inventory-pane", "zengine.inventory-pane"),
                  ("zengine-composer", "zengine.composer"), ("zengine-inventory", "zengine.inventory"),
                  ("zengine-info", "zengine.info")]

        def test_a_part_is_pressed_by_its_name_where_a_picture_draws_it_after_wheeling_to_it(self):
            # The inventory's weave is past Loaded's window: the hand wheels toward the first weave
            # until the words stop changing, then toward the last until Loaded draws the part.
            loaded = CanvasLoaded(self.WEAVES, window=3, origin=1)
            ctx = CanvasWorkshop(steps, {self.LOADED: loaded})
            held = hand.Hand(ctx, "workshop")
            try:
                at = held.part(*self.LOADED, "weave:zengine-inventory", scroll=True)
                held.click(at)
                rows = act.painted(held, *self.LOADED)
            finally:
                held.close()
            self.assertEqual(loaded.chosen, ["zengine-inventory"])
            self.assertEqual((at["name"], at["text"], at["row"]),
                             ("weave:zengine-inventory", "  zengine-inventory @zengine.inventory", 3))
            first = (loaded.column_x(8) + 3, loaded.row_y(0) + 7)  # "loaded weaves -- 6"'s middle
            self.assertEqual([(e["x"], e["y"], e["wheel_dy"]) for e in ctx.events
                              if e["kind"] == "PointerWheel"],
                             [first + (1,), first + (1,), first + (-1,), first + (-1,)])
            presses = [(e["x"], e["y"]) for e in ctx.events if e["kind"] == "PointerButton"]
            self.assertEqual(presses, [(at["x"], at["y"])] * 2)
            # The blank row under the heading is no word: a row's number is its word's, not its row's.
            self.assertEqual([(r["row"], r["text"]) for r in rows["rows"]],
                             [(0, "loaded weaves -- 6"), (1, "  zengine-inventory-pane @zengine.inventory-pane"),
                              (2, "  zengine-composer @zengine.composer"),
                              (3, "  zengine-inventory @zengine.inventory"), (4, "  ... 2 earlier, 1 more")])
            self.assertTrue(rows["canvas"])
            self.assertEqual(set(ctx.asked), {("PaneViewRequested", 3)})
            self.assertFalse(ctx.owner.open)

        def test_a_row_is_found_by_what_a_picture_says_and_pressed_at_its_third_character(self):
            loaded = CanvasLoaded(self.WEAVES, window=3, origin=3)
            ctx = CanvasWorkshop(steps, {self.LOADED: loaded})
            held = hand.Hand(ctx, "workshop")
            try:
                at = held.row(*self.LOADED, "zengine-skin-", scroll=True)
                held.click(at)
                with self.assertRaisesRegex(CheckFailed, "ambiguous visible row: zengine-"):
                    held.row(*self.LOADED, "zengine-")
                with self.assertRaisesRegex(ValueError, "visible row not found: zengine-flow"):
                    held.row(*self.LOADED, "zengine-flow")
                kept = json.loads(ctx.kept["last-view.json"])
                with self.assertRaisesRegex(ValueError, "no visible part weave:zengine-flow"):
                    held.part(*self.LOADED, "weave:zengine-flow", scroll=True)
                named = json.loads(ctx.kept["last-view.json"])["parts"]
            finally:
                held.close()
            # Word 1 is the skin's row, the lattice's row 2: its third character is where it is pressed.
            self.assertEqual(at, {"row": 1, "text": "  zengine-skin-sdl @zengine.skin",
                                  "x": loaded.column_x(2) + 3, "y": loaded.row_y(2) + 7, "space": 2})
            self.assertEqual(loaded.chosen, ["zengine-skin-sdl"])
            self.assertEqual(kept["rows"][0]["text"], "loaded weaves -- 6")
            self.assertEqual(named, ["weave:zengine-composer", "weave:zengine-inventory",
                                     "weave:zengine-info"])
            self.assertEqual(set(ctx.asked), {("PaneViewRequested", 3), ("PanePointRequested", 2)})

        def test_a_row_of_a_text_pane_is_its_word_and_is_pressed_at_its_third_character(self):
            ctx = ActWorkshop(steps, [], [">Width       480", " Height      300", " Placement", " X"])
            held = hand.Hand(ctx, "workshop")
            try:
                at = held.row("zengine.info", "info", "Height")
                short = held.row("zengine.info", "info", " X")
                with self.assertRaisesRegex(CheckFailed, "ambiguous visible row: e"):
                    held.row("zengine.info", "info", "e")
                with self.assertRaisesRegex(ValueError, "visible row not found: Depth"):
                    held.row("zengine.info", "info", "Depth")
            finally:
                held.close()
            self.assertEqual(at, {"row": 1, "text": " Height      300", "x": 101, "y": 2, "space": 2})
            self.assertEqual(short, {"row": 3, "text": " X", "x": 6, "y": 42, "space": 2})
            self.assertEqual(ctx.points, [(1, 2)])
            self.assertEqual([r["text"] for r in json.loads(ctx.kept["last-view.json"])["rows"]],
                             [">Width       480", " Height      300", " Placement", " X"])

        def test_painted_reads_words_as_rows_and_a_pane_not_described_never_holds_a_row(self):
            ctx = ActWorkshop(steps, [], self.INFO_ROWS)
            held = hand.Hand(ctx, "workshop")
            try:
                rows = act.painted(held, "zengine.info", "info")
                self.assertIsNone(act.painted(held, *act.MANAGER))
                started = time.monotonic()
                self.assertEqual(act.wait_rows(held, *act.MANAGER, "PANES", 0.3, present=False),
                                 (None, None))
                self.assertGreaterEqual(time.monotonic() - started, 0.3)
            finally:
                held.close()
            self.assertEqual([(r["row"], r["text"], r["x"], r["y"]) for r in rows["rows"]],
                             [(0, self.INFO_ROWS[0], 6, 6), (1, self.INFO_ROWS[1], 6, 18),
                              (2, self.INFO_ROWS[2], 6, 30)])
            self.assertEqual((rows["picture"], rows["canvas"]), (1, False))

        def test_the_reset_button_is_pressed_by_its_name_in_the_picture_the_controls_draw(self):
            controls = CanvasControls()
            ctx = CanvasWorkshop(steps, {("zengine.demo", "controls"): controls})
            self.assertIn("next generation reached ready", reset_button.run(ctx))
            self.assertEqual(controls.resets, 1)
            presses = [(e["x"], e["y"], e["pressed"]) for e in ctx.events if e["kind"] == "PointerButton"]
            self.assertEqual(presses, [(controls.column_x(0) + 55 * 7 // 2, controls.row_y(1) + 7, True),
                                       (controls.column_x(0) + 55 * 7 // 2, controls.row_y(1) + 7, False)])
            self.assertEqual(ctx.asked, [("PaneViewRequested", 3)])
            self.assertFalse(ctx.owner.open)
            # A reset row every place of which another part takes is never pressed.
            controls = CanvasControls()
            ctx = CanvasWorkshop(steps, {("zengine.demo", "controls"): controls},
                                 unreached={"control:reset"})
            with self.assertRaisesRegex(ValueError, "no press reaches control:reset"):
                reset_button.run(ctx)
            self.assertFalse([e for e in ctx.events if e["kind"] == "PointerButton"])
            self.assertEqual(controls.resets, 0)
            self.assertFalse(ctx.owner.open)

        def test_open_select_and_a_mark_read_a_pane_manager_that_draws_a_picture_by_name(self):
            # One run a row, the same with each row drawn as two runs listed by column, and with
            # each run set below its row's rectangle: the rows are read by the parts they stand on.
            for drawn in ({}, {"split": True}, {"drop": 3}):
                with self.subTest(**drawn):
                    # HIDDEN, AND THE ROW OUT OF THE WINDOW: Ctrl+P, a press on the heading for the
                    # keys, Down until the window draws the row, marked, and one press opening it.
                    manager = CanvasManager(self.LISTED, **drawn)
                    ctx = ActWorkshop(steps, [{"open": "pane:zengine.files/project-files"}], [],
                                      manager=manager)
                    self.act(ctx)
                    record = ctx.done()[0]
                    self.assertEqual((record["opened"], record["presses"], record["state"]),
                                     ("pane:zengine.files/project-files", 1, "covered"))
                    self.assertEqual(manager.open, {("zengine.files", "project-files")})
                    presses = [(e["x"], e["y"]) for e in ctx.events
                               if e["kind"] == "PointerButton" and e["pressed"]]
                    own = manager.column_x(8) + 51 * 7 // 2  # the row's widest stretch past its mark
                    self.assertEqual(presses, [(manager.column_x(0) + 3, manager.row_y(0) + 7),
                                               (own, manager.row_y(4) + 7)])
                    self.assertEqual([e["scancode"] for e in ctx.events if e["kind"] == "KeyPressed"],
                                     [19, 81, 81, 81, 81])
                    # DRAWN AND NOT MARKED: a press choosing it, then one opening it.
                    manager = CanvasManager(self.LISTED, shown=True, **drawn)
                    ctx = ActWorkshop(steps, [{"open": "pane:zengine.terminal/terminal"}], [],
                                      manager=manager)
                    self.act(ctx)
                    self.assertEqual(ctx.done()[0]["presses"], 2)
                    self.assertEqual(manager.open, {("zengine.terminal", "terminal")})
                    # ALREADY MARKED, WITH THE KEYS THERE: one press opens it.
                    manager = CanvasManager(self.LISTED, shown=True, cursor=1, keys=True, **drawn)
                    ctx = ActWorkshop(steps, [{"open": "pane:zengine.desktop/hotkeys"}], [],
                                      manager=manager)
                    self.act(ctx)
                    self.assertEqual(ctx.done()[0]["presses"], 1)
                    self.assertEqual(manager.open, {("zengine.desktop", "hotkeys")})
                    # SELECT BY NAME walks the marker there; a press on a row's mark chooses its row.
                    manager = CanvasManager(self.LISTED, shown=True, keys=True, **drawn)
                    ctx = ActWorkshop(steps, [
                        {"select": ["zengine.desktop", "launcher", "pane:zengine.terminal/terminal"]},
                        {"part": ["zengine.desktop", "launcher", "mark:zengine.info/info"]}], [],
                        manager=manager)
                    self.act(ctx)
                    selected, marked = ctx.done()
                    self.assertEqual((selected["chosen"], selected["presses"]), ("> [    ] Terminal", 2))
                    self.assertEqual((marked["text"], marked["x"], marked["y"]),
                                     ("[    ]", manager.column_x(2) + 21, manager.row_y(2) + 7))
                    self.assertEqual((manager.cursor, manager.open), (0, set()))

        def test_wheel_turns_over_a_named_part_or_a_panes_first_word(self):
            ctx = ActWorkshop(steps, [{"wheel": ["zengine.info", "info", "property:Height"], "dy": -1}],
                              self.INFO_ROWS, named=self.INFO_NAMES)
            self.act(ctx)
            self.assertEqual([(e["x"], e["y"], e["space"], e["wheel_dx"], e["wheel_dy"])
                              for e in ctx.events if e["kind"] == "PointerWheel"], [(50, 18, 2, 0.0, -1.0)])
            self.assertEqual(len([e for e in ctx.events if e["kind"] != "PointerWheel"]), 0)
            self.assertEqual(ctx.done()[0]["part"], "property:Height")
            ctx = ActWorkshop(steps, [{"wheel": ["zengine.editor", "editor"], "dy": 2, "dx": 0.5}], [],
                              panes={"editor": ["notes", "more"]})
            self.act(ctx)
            self.assertEqual([(e["x"], e["y"], e["wheel_dx"], e["wheel_dy"]) for e in ctx.events],
                             [(6, 6, 0.5, 2.0)])
            ctx = ActWorkshop(steps, [{"wheel": ["zengine.info", "info", "property:Depth"], "dy": 1,
                                       "seconds": 0}], self.INFO_ROWS, named=self.INFO_NAMES)
            with self.assertRaisesRegex(CheckFailed, "draws no part named 'property:Depth'"):
                self.act(ctx)
            self.assertEqual(json.loads(ctx.kept["failed-step-parts.json"]), self.INFO_NAMES["info"])
            ctx = ActWorkshop(steps, [{"wheel": ["zengine.info", "info", "property:Height"], "dy": 1}],
                              self.INFO_ROWS, named=self.INFO_NAMES, unreached={"property:Height"})
            with self.assertRaisesRegex(CheckFailed, "no point reaches 'property:Height'"):
                self.act(ctx)
            self.assertFalse(ctx.events)

        def test_a_wheel_is_spelled_before_any_contact(self):
            for wheel in ({"wheel": ["zengine.info", "info"]}, {"wheel": ["zengine.info"], "dy": 1},
                          {"wheel": "zengine.info/info", "dy": 1},
                          {"wheel": ["zengine.info", "info"], "dy": "1"},
                          {"wheel": ["zengine.info", "info"], "dy": True},
                          {"wheel": ["zengine.info", "info"], "dx": float("nan")},
                          {"wheel": ["zengine.info", "info"], "dy": float("inf")}):
                with self.subTest(wheel=wheel):
                    ctx = ActWorkshop(steps, [{"press": "down"}, wheel], [])
                    with self.assertRaisesRegex(ValueError, "wheel"):
                        self.act(ctx)
                    self.assertEqual(ctx.contacts, [])

        def test_a_wheel_scrolls_a_pane_that_draws_a_picture(self):
            loaded = CanvasLoaded(self.WEAVES, window=3, origin=0)
            ctx = CanvasWorkshop(steps, {self.LOADED: loaded}, act_steps=[
                {"wheel": list(self.LOADED), "dy": -1},
                {"expect": list(self.LOADED) + ["1 earlier"], "seconds": 0},
                {"wheel": list(self.LOADED) + ["weave:zengine-skin-sdl"], "dy": 1},
                {"absent": list(self.LOADED) + ["1 earlier"], "seconds": 0}])
            self.act(ctx)
            wheels = [(e["x"], e["y"], e["wheel_dy"]) for e in ctx.events if e["kind"] == "PointerWheel"]
            self.assertEqual(wheels, [(loaded.column_x(8) + 3, loaded.row_y(0) + 7, -1.0),
                                      (loaded.column_x(0) + 59 * 7 // 2, loaded.row_y(2) + 7, 1.0)])
            self.assertEqual(loaded.origin, 0)
            self.assertEqual(set(ctx.asked), {("PaneViewRequested", 3)})

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ToolChecks)
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite).wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True)
    parser.add_argument("--runtime", required=True)
    args = parser.parse_args()
    sys.exit(0 if run_checks(args.tools, args.runtime) else 1)
