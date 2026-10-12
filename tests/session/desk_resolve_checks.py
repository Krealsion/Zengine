# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Checks of `desk/resolve` (external-host/tools/desk) against a scripted desk: the keymap's rows
with what a press of each may cause, each pane's other modes, the desk, the inventory and each
pane's parts, as Workshop answers them. The checks read what the resolution says: a control shown
and usable; a pane offered and hidden, reached through the Pane Manager and the desktop's own key;
a control in the pane's other mode, reached through the row that enters it; a message no control
causes, the case for a tool for a row that reads every pane and the panes outside its read for one
that does not; a route that never runs through the Hotkeys pane; and the mark asked by the address a
press names. They claim nothing about a real Workshop.

Standalone: python desk_resolve_checks.py --tools <external-host/tools/desk> --runtime <loom runtime>
Pure Python; no Workshop and no Loom host run.
"""

import argparse
import importlib
from pathlib import Path
import sys
import unittest

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
