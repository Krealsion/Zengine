# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Checks of `desk/resolve` (external-host/tools/desk) against a scripted desk: the keymap's rows
with what a press of each may cause, each pane's other modes, the desk, the inventory and each
pane's parts, as Workshop answers them. The checks read what the resolution says: a control shown
and usable; a pane offered and hidden, reached through the Pane Manager and the desktop's own key;
a control in the pane's other mode, reached through the row that enters it; a message no control
causes, the case for a tool for a row that reads every pane and the panes outside its read for one
that does not; a shape nothing names, said unknown; a cause with no subject, no route to show a
pane; a route that never runs through the Hotkeys pane; the mark asked by the address a press
names, and why nothing was marked; the typed inputs and `resolve.json`. They claim nothing about a
real Workshop.

Standalone: python desk_resolve_checks.py --tools <external-host/tools/desk> --runtime <loom runtime>
Pure Python; no Workshop and no Loom host run.
"""

import argparse
import importlib
import json
from pathlib import Path
import sys
import unittest

#: The package the checks read its manifest from, set by `run_checks`.
TOOLS = None
CELLS = 1
BUILDER = ("zengine.builder-pane", "builder")
LAUNCHER = ("zengine.desktop", "launcher")
HOTKEYS = ("zengine.desktop", "hotkeys")


def cause(shape, version, subject=""):
    return {"shape": shape, "version": version, "subject": subject}


def binding(office, pane, ident, key, causes=(), enters=""):
    return {"office": office, "pane": pane, "id": ident, "gesture": key, "shown": key,
            "causes": list(causes), "enters": enters, "label": ident}


APP_ROWS = [
    binding("zengine.desktop", "", "desktop.panes", "ctrl+p",
            [cause("PaneToggleRequested", 1, "to zengine.desktop/launcher")]),
    binding("zengine.desktop", "", "desktop.hotkeys", "ctrl+k",
            [cause("PaneLaunchRequested", 1, "to zengine.desktop/hotkeys")]),
]
BROWSE = [
    binding(*BUILDER, ident="builder.build", key="b", causes=[cause("BuildRequested", 2)]),
    binding(*BUILDER, ident="builder.output", key="l", causes=[cause("BuildOutputRequested", 1)],
            enters="output"),
]
OUTPUT = [binding(*BUILDER, ident="builder.output-close", key="escape", enters="browse")]
LAUNCHER_ROWS = [
    binding(*LAUNCHER, ident="launcher.open", key="return",
            causes=[cause("PaneLaunchRequested", 1, "on pane:")]),
]
HOTKEYS_ROWS = [binding(*HOTKEYS, ident="hotkeys.commit", key="return",
                        causes=[cause("BuildRequested", 2)])]


def part(name, available=True, shown=True):
    return {"name": name, "text": "[" + name + "]", "available": available,
            "space": CELLS if shown else 0, "x": 1, "y": 1}


class Scripted(object):
    """One desk as a script sets it: which panes are on it, offered, and what each shows."""

    def __init__(self):
        self.row = {"name": "tutor", "by_door": True, "capture": ["*"]}
        self.rows = list(APP_ROWS)
        self.modes = []
        self.on_desk = []
        self.offered = [BUILDER, LAUNCHER, HOTKEYS]
        self.parts = {}

    def desk(self, resolve):
        def read_view(office, pane):
            return {"provider": office, "pane": pane, "parts": self.parts.get((office, pane), [])}
        inventory = {"panes": [{"office": o, "pane": p, "name": p.title(), "open": (o, p) in self.on_desk,
                                "available": True} for o, p in self.offered]}
        desk = {"panes": [{"provider": o, "pane": p, "name": p.title(), "state": "open"}
                          for o, p in self.on_desk]}
        return resolve.Desk(self.row, {"rows": self.rows, "modes": self.modes}, desk, inventory,
                            read_view)


class FakeAsker(object):
    def __init__(self, answer):
        self.answer, self.asked = answer, []

    def ask(self, office, shape, version, fields):
        self.asked.append((office, shape, version, fields))
        return self.answer, None


class FakeCtx(object):
    """A run's inputs as the manager types them, and the outputs it produces."""

    def __init__(self, inputs):
        self.inputs, self.produced, self.steps = dict(inputs), {}, []

    def step(self, name, note=""):
        self.steps.append(name)

    def produce(self, name, data):
        self.produced[name] = data
        return name

    def fail(self, message):
        raise AssertionError("the run failed: " + message)


def make_checks(resolve):
    class ResolveChecks(unittest.TestCase):
        def test_a_control_shown_and_usable_is_the_one_answer_and_is_marked_by_its_address(self):
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            s.parts[BUILDER] = [part("control:builder.build")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            self.assertEqual([c["address"] for c in r["ready"]],
                             ["zengine.builder-pane/builder:builder.build"])
            self.assertEqual(r["ready"][0]["key"], "b")
            asker = FakeAsker({"shown": True, "said": "marked"})
            line, address = resolve.mark(asker, r, "build this one")
            self.assertEqual(address, "zengine.builder-pane/builder:builder.build")
            self.assertEqual(asker.asked[0][1:3], ("GuideRequested", 1))
            self.assertEqual(asker.asked[0][3]["pane"], "zengine.builder-pane/builder")
            self.assertEqual(asker.asked[0][3]["control"], "builder.build")

        def test_an_unusable_control_is_said_and_not_marked(self):
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            s.parts[BUILDER] = [part("control:builder.build", available=False)]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            self.assertEqual(r["ready"], [])
            self.assertIs(r["found"][0]["usable"], False)
            self.assertIn("unusable now", resolve.said(r["found"][0]))

        def test_a_hidden_pane_is_reached_through_the_pane_manager_and_the_desktops_own_key(self):
            s = Scripted()
            s.rows = APP_ROWS + BROWSE + LAUNCHER_ROWS
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            self.assertEqual(r["found"][0]["place"], "hidden")
            step = r["steps"][0]
            self.assertEqual(step["to show"], "zengine.builder-pane/builder")
            # THE PANE MANAGER IS NOT OPEN: its own showing is the desktop's key, a row with no part.
            deeper = resolve.flatten(step["then"])
            self.assertTrue(any(c["address"] == "zengine.desktop:desktop.panes" and c["key"] == "ctrl+p"
                                for st in deeper for c in st["by"]))
            line, address = resolve.mark(FakeAsker({"shown": True}), r, "")
            self.assertIsNone(address)
            self.assertIn("not marked", line)

        def test_with_the_pane_manager_open_its_row_for_the_pane_is_the_nearest_step_and_is_marked(self):
            s = Scripted()
            s.on_desk = [LAUNCHER]
            s.rows = APP_ROWS + BROWSE + LAUNCHER_ROWS
            s.parts[LAUNCHER] = [part("pane:zengine.builder-pane/builder")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            by = r["steps"][0]["by"]
            self.assertEqual([c["address"] for c in by], ["zengine.desktop/launcher:launcher.open"])
            self.assertEqual(by[0]["part"], "pane:zengine.builder-pane/builder")
            asker = FakeAsker({"shown": True, "said": "marked"})
            line, address = resolve.mark(asker, r, "open the Builder here")
            self.assertEqual(address, "zengine.desktop/launcher:launcher.open")
            self.assertEqual(asker.asked[0][3]["control"], "part:pane:zengine.builder-pane/builder")

        def test_a_control_in_another_mode_is_reached_through_the_row_that_enters_it_never_a_case_for_a_tool(self):
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + OUTPUT
            s.modes = [{"office": BUILDER[0], "pane": BUILDER[1], "mode": "output",
                        "elsewhere": [{"mode": "browse", "causes": [cause("BuildRequested", 2)]}]}]
            s.parts[BUILDER] = [part("control:builder.output-close")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            self.assertFalse(r["nowhere"])
            step = r["steps"][0]
            self.assertEqual(step["to show"], "zengine.builder-pane/builder in its mode browse")
            self.assertEqual([c["address"] for c in step["by"]],
                             ["zengine.builder-pane/builder:builder.output-close"])
            line, address = resolve.mark(FakeAsker({"shown": True}), r, "")
            self.assertEqual(address, "zengine.builder-pane/builder:builder.output-close")

        def test_a_message_no_control_causes_is_the_case_for_a_tool_for_a_row_that_reads_every_pane(self):
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            r = resolve.resolve(s.desk(resolve), "StatusRequested", 1, "")
            self.assertTrue(r["nowhere"])
            claims = resolve.case_for_a_tool(s.desk(resolve), "StatusRequested", 1, "zengine.builder",
                                             "the lesson asks where the Builder stands",
                                             "zengine.builder-pane/builder")
            self.assertEqual([c[:2] for c in claims[:4]], ["1.", "2.", "3.", "4."])
            self.assertIn("filed nowhere", claims[4])

        def test_a_row_that_reads_by_pane_is_told_the_panes_outside_its_read_never_a_case_for_a_tool(self):
            s = Scripted()
            s.row = {"name": "tutor", "by_door": True, "capture": ["zengine.builder-pane/*"]}
            s.rows = APP_ROWS + BROWSE  # the Pane Manager's rows are withheld: outside the read
            r = resolve.resolve(s.desk(resolve), "PaneLaunchRequested", 1, "zengine.builder-pane/builder")
            self.assertFalse(r["nowhere"])
            self.assertTrue(any("zengine.desktop/launcher" in o for o in r["outside"]))

        def test_a_shape_nothing_names_is_unknown_apart_from_a_known_shape_no_control_causes(self):
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            vocabulary = {("InventoryCaptureAdd", 1)}
            known = lambda shape, version: (shape, version or 1) in vocabulary
            nowhere = resolve.resolve(s.desk(resolve), "NoSuchShapeXyz", 0, "", known)
            self.assertTrue(nowhere["unknown"])
            self.assertFalse(nowhere["nowhere"])
            words = resolve.lines(s.desk(resolve), nowhere, "NoSuchShapeXyz", 0, "", {})
            self.assertIn("unknown", words[0])
            self.assertFalse(any("case for a tool" in w for w in words))
            real = resolve.resolve(s.desk(resolve), "InventoryCaptureAdd", 1, "", known)
            self.assertFalse(real["unknown"])
            self.assertTrue(real["nowhere"])
            self.assertNotIn("unknown", resolve.lines(s.desk(resolve), real, "InventoryCaptureAdd", 1,
                                                      "", {})[0])
            # A shape a row in force declares is known, whatever the vocabulary says.
            declared = resolve.resolve(s.desk(resolve), "BuildOutputRequested", 1, "",
                                       lambda shape, version: False)
            self.assertFalse(declared["unknown"])

        def test_a_mark_that_marks_nothing_says_why(self):
            s = Scripted()
            s.rows = APP_ROWS + BROWSE + LAUNCHER_ROWS
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            line, address = resolve.mark(FakeAsker({"shown": True}), r, "")
            self.assertIsNone(address)
            # The steps the lines list are a key with no part: say so, and name the key.
            self.assertIn("no part to mark", line)
            self.assertIn("ctrl+p", line)
            s = Scripted()
            s.on_desk = [LAUNCHER]
            s.rows = APP_ROWS + BROWSE + LAUNCHER_ROWS + [
                binding(*LAUNCHER, ident="launcher.show", key="space",
                        causes=[cause("PaneLaunchRequested", 1, "on pane:")])]
            s.parts[LAUNCHER] = [part("pane:zengine.builder-pane/builder")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            line, address = resolve.mark(FakeAsker({"shown": True}), r, "")
            self.assertIsNone(address)
            self.assertIn("2 controls shown and usable", line)
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            s.parts[BUILDER] = [part("control:builder.build")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")

            class Refusing(FakeAsker):
                def ask(self, office, shape, version, fields):
                    return None, "dispatch-refused: CapabilityDenied"
            line, address = resolve.mark(Refusing(None), r, "")
            self.assertIsNone(address)
            self.assertIn("CapabilityDenied", line)

        def test_a_cause_with_no_subject_is_no_route_to_show_another_pane(self):
            info = ("zengine.info", "view")
            s = Scripted()
            s.on_desk = [info]
            s.offered = [BUILDER, LAUNCHER, HOTKEYS, info]
            s.rows = APP_ROWS + BROWSE + [
                binding(*info, ident="info.view.new", key="n", causes=[cause("PaneLaunchRequested", 1)]),
                binding(*info, ident="info.view.fork", key="f", causes=[cause("PaneLaunchRequested", 1)])]
            s.parts[info] = [part("control:info.view.new"), part("control:info.view.fork")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            routes = [c["address"] for st in resolve.flatten(r["steps"]) for c in st["by"]]
            self.assertNotIn("zengine.info/view:info.view.new", routes)
            self.assertNotIn("zengine.info/view:info.view.fork", routes)
            # Asked with no subject, the same causes still answer.
            r = resolve.resolve(s.desk(resolve), "PaneLaunchRequested", 1, "")
            self.assertIn("zengine.info/view:info.view.new", [c["address"] for c in r["found"]])

        def test_version_and_mark_are_typed_and_the_whole_resolution_is_written(self):
            manifest = json.loads((TOOLS / "loom-tool.json").read_text(encoding="utf-8"))
            tool = [t for t in manifest["tools"] if t["name"] == "resolve"][0]
            types = dict((i["name"], i["type"]) for i in tool["inputs"])
            self.assertEqual(types["version"], "int")
            self.assertEqual(types["mark"], "bool")
            self.assertIn("resolve.json", [o["name"] for o in tool["outputs"]])
            s = Scripted()
            s.on_desk = [BUILDER]
            s.rows = APP_ROWS + BROWSE
            s.parts[BUILDER] = [part("control:builder.build")]
            ctx = FakeCtx({"shape": "BuildRequested", "version": 2, "mark": True})
            summary = resolve.answer(ctx, s.desk(resolve), FakeAsker({"shown": True, "said": "ok"}))
            written = json.loads(ctx.produced["resolve.json"].decode("utf-8"))
            self.assertEqual([c["address"] for c in written["ready"]],
                             ["zengine.builder-pane/builder:builder.build"])
            self.assertEqual(written["mark"]["address"], "zengine.builder-pane/builder:builder.build")
            self.assertIn("resolve.json", summary)

        def test_no_route_runs_through_the_hotkeys_pane(self):
            s = Scripted()
            s.on_desk = [HOTKEYS]
            s.rows = APP_ROWS + HOTKEYS_ROWS
            s.parts[HOTKEYS] = [part("control:hotkeys.commit")]
            r = resolve.resolve(s.desk(resolve), "BuildRequested", 2, "")
            self.assertEqual(r["found"], [])
            r = resolve.resolve(s.desk(resolve), "PaneLaunchRequested", 1, "zengine.desktop/hotkeys")
            self.assertEqual(r["found"], [])

    return ResolveChecks


def run_checks(tools, runtime):
    global TOOLS
    TOOLS = Path(tools).resolve()
    sys.path[:0] = [str(Path(runtime).resolve()), str(Path(tools).resolve())]
    resolve = importlib.import_module("resolve")
    suite = unittest.TestLoader().loadTestsFromTestCase(make_checks(resolve))
    result = unittest.TextTestRunner(verbosity=1).run(suite)
    return result.wasSuccessful() and result.testsRun > 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--tools", required=True)
    parser.add_argument("--runtime", required=True)
    a = parser.parse_args()
    sys.exit(0 if run_checks(a.tools, a.runtime) else 1)
