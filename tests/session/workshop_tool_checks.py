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
        fields = [("kind", types.TEXT), ("scancode", types.INT), ("modifiers", types.INT),
                  ("text", types.TEXT), ("button", types.INT), ("pressed", types.BOOL),
                  ("x", types.INT), ("y", types.INT), ("space", types.INT),
                  ("dx", types.INT), ("dy", types.INT)]
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


class ActWorkshop(Context):
    """A run context whose Workshop is a script for `workshop/act`: Info's list with a cursor the
    Down and Up keys move, a canvas pane's words, a point door that says which word and column it
    was asked for, and a desk with a menu open. Every injected moment is kept, as `Context` keeps
    them."""

    def __init__(self, steps, act_steps, rows):
        Context.__init__(self, steps)
        self.inputs["steps"] = json.dumps(act_steps)  # the input shares the module's name
        self.base = list(rows)
        self.cursor = next((i for i, r in enumerate(rows) if r.startswith(">")), 0)
        self.points, self.kept = [], {}

    def rows(self):
        return [(">" if i == self.cursor else " ") + r[1:] for i, r in enumerate(self.base)]

    def produce(self, name, data):
        self.kept[name] = data

    def done(self):
        return json.loads(self.kept["steps.json"])

    def ask(self, office, shape, fields, **options):
        if shape == "InjectInput":
            for e in fields["events"]:
                if e["kind"] == "KeyPressed" and e["scancode"] in (81, 82):
                    step = 1 if e["scancode"] == 81 else -1
                    self.cursor = max(0, min(len(self.base) - 1, self.cursor + step))
        if shape == "PaneViewRequested" and options.get("version") == 2:
            canvas = fields["pane"] == "view-builder"
            texts = ["node one", "[Label]"] if canvas else self.rows()
            return {"provider": fields["provider"], "pane": fields["pane"], "picture": 1,
                    "canvas": canvas, "words": [
                        {"word": i, "text": t, "place": {"x": 0, "y": 12 * i, "w": 12 * len(t), "h": 12},
                         "x": 6, "y": 12 * i + 6, "space": 2} for i, t in enumerate(texts)]}
        if shape == "PanePointRequested" and options.get("version") == 2:
            self.points.append((fields["word"], fields["column"]))
            return {"x": 100 + fields["word"], "y": fields["column"], "space": 2}
        if shape == "DeskViewRequested":
            place = {"x": 0, "y": 24, "w": 480, "h": 300}
            return {"width": 1440, "height": 900, "cell_px": 12, "space": 2,
                    "room": {"x": 0, "y": 24, "w": 1440, "h": 800}, "arranging": False,
                    "panes": [{"provider": "zengine.info", "pane": "info", "name": "Info",
                               "state": "open", "front": 0, "selected": True, "keys": True,
                               "visible": place, "resolved": place}],
                    "menu": {"open": True, "office": "zengine.files", "pane": "files", "picture": 3,
                             "place": {"x": 20, "y": 30, "w": 100, "h": 40},
                             "lines": [{"word": 0, "text": "> Rename", "x": 30, "y": 41, "space": 2},
                                       {"word": 1, "text": "  Delete", "x": 30, "y": 59, "space": 2}]}}
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

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ToolChecks)
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite).wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True)
    parser.add_argument("--runtime", required=True)
    args = parser.parse_args()
    sys.exit(0 if run_checks(args.tools, args.runtime) else 1)
