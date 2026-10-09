# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Checks of `desk/watch` (external-host/tools/desk) against a scripted Workshop whose desk moves
between notices, and a scripted subscription telling them.

The desk holds a few panes, each with its own words and a fingerprint naming what it shows; a
script moves it -- a pane's words, the selection, a pane closed -- and says each move as a
`DeskStamps` notice, the whole state, as Workshop does. The checks read what the watch wrote and
asked: AGENTS.md first; the subscription made before the first read and named `latest`; only
what moved read again -- a pane alone when its stamp moved, the desk when its number moved, paced;
a notice the reading already stands on writing nothing and asking nothing; each change the
smaller of its changed lines and its lines as they stand; the ring of the last 64 changes within
64 KiB saying what it dropped, continued by the next watch and started again over a file it did
not write; a gap read again whole and said; two watches of different panes each writing only
their own; a write a reader's open file refuses written at the next change; a row the relay
refuses failing in words. They claim nothing about a real Workshop.

Standalone: python desk_watch_checks.py --tools <external-host/tools/desk> --runtime <loom runtime>
Pure Python; no Workshop and no Loom host run.
"""

import argparse
import copy
import importlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

PIXELS = 2


def rect(x, y, w, h):
    return {"x": x, "y": y, "w": w, "h": h}


def word(n, text, x, y, h=18):
    return {"word": n, "text": text, "place": rect(x, y, 8 * len(text), h),
            "x": x + 4 * len(text), "y": y + h // 2, "space": PIXELS}


ROW = {"name": "watcher", "may": ["input", "capture"],
       "observe": [{"producer": "zengine.workshop", "shape": "DeskStamps", "version": 1}],
       "host": "development"}


class Pane(object):
    """One pane of the scripted desk: its place, its words, and the fingerprint naming them."""

    def __init__(self, provider, pane, name, x, lines):
        self.provider, self.pane, self.name = provider, pane, name
        self.x = x
        self.lines = list(lines)
        self.print = 1000 + x
        self.selected = False

    def ident(self):
        return (self.provider, self.pane)

    def stamp(self):
        return {"provider": self.provider, "pane": self.pane, "holder": 7, "incarnation": 1,
                "grant": 0, "fingerprint": self.print}

    def show(self, lines):
        self.lines = list(lines)
        self.print += 1

    def view(self):
        words = [word(i, t, self.x + 3, 60 + 18 * i) for i, t in enumerate(self.lines)]
        s = self.stamp()
        return {"provider": self.provider, "pane": self.pane, "picture": 0, "canvas": False,
                "words": words, "parts": [], "holder": s["holder"],
                "incarnation": s["incarnation"], "grant": s["grant"],
                "fingerprint": s["fingerprint"],
                "covered": {"by": [], "words": 0, "rect": rect(0, 0, 0, 0)}, "in_flight": False,
                "from": 0, "total": len(words)}


class Desk(object):
    """The scripted Workshop: a desk that moves when the script says, answering every ask from
    what it holds now, and keeping each ask."""

    def __init__(self):
        self.refusals = 0  # desk reads to refuse, as Workshop's rate does
        self.number = 5
        self.panes = [Pane("zengine.info", "info", "Info", 0, ["PANES -- 3", "info line"]),
                      Pane("td.game", "board", "Board", 500, ["wave 1", "enemy at 3"]),
                      Pane("zengine.files", "project-files", "Files", 1000,
                           ["src/", "README.md"])]

    def pane(self, ident):
        return next(p for p in self.panes if p.ident() == ident)

    def move(self):
        self.number += 1

    def stamps(self):
        return {"desk": self.number, "panes": [p.stamp() for p in self.panes]}

    def desk_read(self):
        panes = [{"provider": p.provider, "pane": p.pane, "name": p.name, "state": "open",
                  "front": i, "resolved": rect(p.x, 42, 480, 500),
                  "visible": rect(p.x, 42, 480, 500), "selected": p.selected,
                  "keys": p.selected} for i, p in enumerate(self.panes)]
        desk = {"desk": self.number, "width": 1920, "height": 1080, "cell_px": 0,
                "space": PIXELS, "room": rect(0, 42, 1920, 1000), "panes": panes,
                "arranging": False, "menu": {"open": False}, "words": [], "slots": []}
        return {"desk": desk, "stamps": [p.stamp() for p in self.panes],
                "panes": [p.view() for p in self.panes]}


class Clock(object):
    """The watch's time, moved only by its own pauses and by a wait the script has nothing for."""

    def __init__(self):
        self.now = 0.0
        self.slept = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.slept.append(seconds)
        self.now += seconds


class Subscription(object):
    """A scripted subscription: each step of the script moves the desk and says what the relay
    would -- a notice of the whole state, a gap, an ending -- in order, numbered. Past the script,
    a wait runs out."""

    def __init__(self, tool_items, desk, script, clock):
        self.items, self.desk, self.script, self.clock = tool_items, desk, list(script), clock
        self.seq = 0
        self.ended = None
        self.released = False
        self.waiting = []

    def say(self, kind, **given):
        self.seq += 1
        if kind == "observed":
            return self.items.Observation("DeskStamps", 1, copy.deepcopy(self.desk.stamps()),
                                          {"seq": self.seq}, 0.0)
        if kind == "gap":
            return self.items.Gap(self.seq, given.get("lost", 1), "lost on the way", True)
        self.ended = self.items.Ended({"seq": self.seq, "kind": given.get("how", "revoked")})
        return self.ended

    def next(self, timeout=None):
        if self.waiting:
            return self.waiting.pop(0)
        if not self.script:
            self.clock.now += timeout or 0.0
            return None
        step = self.script.pop(0)
        out = []
        for s in step(self.desk) or ["observed"]:
            if callable(s):  # the desk moves again between two of the step's words
                s(self.desk)
            else:
                out.append(self.say(*s) if isinstance(s, tuple) else self.say(s))
        self.waiting = out[1:]
        return out[0]

    def drain(self):
        out, self.waiting = self.waiting, []
        return out

    def release(self, timeout=15.0):
        self.released = True


class Workshop(object):
    """The run's context: the scripted desk behind its asks and its one subscription."""

    name = "desk-watch-check"

    def __init__(self, tool, items, clock, out, script, desk=None, seconds=60, changes=0, panes="",
                 refuse=None):
        self.tool, self.items, self.clock = tool, items, clock
        self.desk = desk or Desk()
        self.script = script
        self.inputs = {"out": out, "link": "workshop", "seconds": seconds, "changes": changes,
                       "panes": panes}
        self.out = out
        self.asked = []
        self.observed = []
        self.at_first_contact = None
        self.refuse = refuse
        self.sub = None
        self.cleanups = []

    def check(self, condition, message):
        if not condition:
            raise self.tool.ToolFailed(message)

    def fail(self, message):
        raise self.tool.ToolFailed(message)

    def step(self, name, note=""):
        pass

    def note(self, text):
        pass

    def on_cleanup(self, action, label):
        self.cleanups.append(action)

    def contact(self):
        if self.at_first_contact is None:
            self.at_first_contact = sorted(os.path.relpath(os.path.join(d, f), self.out)
                                           for d, _, fs in os.walk(self.out) for f in fs)

    def observe(self, producer, shapes, via=None, latest=(), window=0, label="", max_pending=None,
                timeout=30.0):
        self.contact()
        self.observed.append((producer, list(shapes), via, list(latest), len(self.asked)))
        if self.refuse:
            raise self.tool.Refused(self.refuse)
        self.sub = Subscription(self.items, self.desk, self.script, self.clock)
        return self.sub

    def ask(self, office, shape, fields=None, version=1, via=None, settle=False, timeout=60.0):
        self.contact()
        self.asked.append((shape, version, copy.deepcopy(fields)))
        assert via == "workshop", via
        if shape == "GuestRowDescribedRequested":
            return copy.deepcopy(ROW)
        if shape == "DeskReadRequested":
            assert version == 2
            if self.desk.refusals:
                self.desk.refusals -= 1
                raise self.tool.Refused("desk read refused: at most 4 a second, each asker -- read "
                                        "again in 400 ms")
            return copy.deepcopy(self.desk.desk_read())
        if shape == "PaneViewRequested":
            assert version == 5
            p = self.desk.pane((fields["provider"], fields["pane"]))
            s = fields["stamp"]
            if s["holder"] and s["fingerprint"] != p.print:
                raise self.tool.Refused("pane view stale: read it again from the start")
            return copy.deepcopy(p.view())
        if shape == "PaneInventoryRequested":
            return {"panes": []}
        if shape == "KeymapRequested":
            return {"rows": [], "file": "", "word": ""}
        raise AssertionError(shape)

    def shapes_asked(self):
        return [a[0] for a in self.asked]


# ---- script steps: each moves the desk and says what the relay tells --------------------------

def words_of(ident, lines):
    def step(desk):
        desk.pane(ident).show(lines)
    return step


def select(ident):
    def step(desk):
        for p in desk.panes:
            p.selected = p.ident() == ident
        desk.move()
    return step


def caret(ident):
    """The pane's caret or a colour moved: its stamp, and no line it says."""
    def step(desk):
        desk.pane(ident).print += 1
    return step


def shift(ident, dx):
    """The pane moved: its place, and so every word's, with nothing it shows changed."""
    def step(desk):
        desk.pane(ident).x += dx
        desk.move()
    return step


def refused_then(ident, lines):
    """The desk moves and the pane's words change; the next desk read is refused for its rate."""
    def step(desk):
        desk.pane(ident).show(lines)
        desk.move()
        desk.refusals = 2
    return step


def close(ident):
    def step(desk):
        desk.panes = [p for p in desk.panes if p.ident() != ident]
        desk.move()
    return step


def nothing(desk):
    return None


def gap(desk):
    desk.pane(("zengine.info", "info")).show(["PANES -- 4", "after the gap"])
    return [("gap",)]


def ended(desk):
    return [("ended",)]


def twice(desk):
    """Two moves, each told, before the reader takes either: Info's alone, then the game's too.
    The newest notice stands for both."""
    desk.pane(("zengine.info", "info")).show(["PANES -- 9", "info line"])

    def then(moved):
        moved.pane(("td.game", "board")).show(["wave 2", "enemy at 1"])
    return ["observed", then, "observed"]


INFO = ("zengine.info", "info")
GAME = ("td.game", "board")
FILES = ("zengine.files", "project-files")


def run_checks(tools, runtime):
    sys.path[:0] = [str(Path(runtime).resolve()), str(Path(tools).resolve())]
    watch = importlib.import_module("watch")
    changes = importlib.import_module("desk_changes")
    words_module = importlib.import_module("desk_words")
    from loom_session import observe as items
    from loom_session import tool as loom_tool
    tool = type("Tool", (), {"ToolFailed": loom_tool.ToolFailed, "Refused": loom_tool.Refused})

    def text(path):
        with open(path, "r", encoding="utf-8") as f:
            return f.read()

    class DeskWatchChecks(unittest.TestCase):
        def setUp(self):
            self.temp = tempfile.TemporaryDirectory()
            self.root = os.path.realpath(self.temp.name)
            self.out = os.path.join(self.root, "readings")
            self.clock = Clock()
            timing = patch.object(watch, "time", self.clock)
            timing.start()
            self.addCleanup(timing.stop)

        def tearDown(self):
            self.temp.cleanup()

        def watch(self, script, out=None, **given):
            ctx = Workshop(tool, items, self.clock, out or self.out, script, **given)
            try:
                return ctx, watch.run(ctx)
            finally:  # as a run's cleanup does, however it ended
                for action in ctx.cleanups:
                    action()

        def file(self, *rel, out=None):
            return text(os.path.join(out or self.out, *rel))

        def entries(self, out=None):
            return [l for l in self.file("changes.txt", out=out).splitlines()
                    if l.startswith("== change ")]

        def test_agents_md_first_then_the_subscription_then_each_notice_a_change(self):
            ctx, summary = self.watch([words_of(INFO, ["PANES -- 3", "info changed"]),
                                       select(GAME)])
            self.assertEqual(ctx.at_first_contact, [".desk-watch", "AGENTS.md"])
            self.assertEqual(self.file("AGENTS.md"), words_module.AGENTS_MD)
            # SUBSCRIBED BEFORE THE FIRST READ, the notice named whole state.
            producer, shapes, via, latest, asked_before = ctx.observed[0][:5]
            self.assertEqual((producer, via, latest, asked_before),
                             ("zengine.workshop", "workshop", ["DeskStamps"], 0))
            self.assertEqual(shapes, [("DeskStamps", 1)])
            entries = self.entries()
            self.assertEqual(len(entries), 2, self.file("changes.txt"))
            self.assertTrue(entries[0].startswith("== change 1 -- notice 1: desk 5"))
            self.assertTrue(entries[1].startswith("== change 2 -- notice 2: desk 6"))
            ring = self.file("changes.txt")
            self.assertIn("-- pane zengine.info/info", ring)
            self.assertIn("3,87 info changed", ring.split("== change 2")[0])
            # ...THE READINGS KEPT CURRENT: the desk as it now stands.
            self.assertIn("3,87 info changed", self.file("desk.txt"))
            self.assertIn("td.game/board", [l for l in self.file("desk.txt").splitlines()
                                            if l.endswith(" selected keys")][0])
            self.assertIn("2 change(s) written to changes.txt (changes 1 to 2)", summary)
            self.assertTrue(summary.startswith("Read %s first." % os.path.join(self.out,
                                                                               "AGENTS.md")))
            self.assertTrue(ctx.sub.released)

        def test_only_what_moved_is_read_again(self):
            ctx, _ = self.watch([words_of(GAME, ["wave 1", "enemy at 4"])])
            after_first = ctx.shapes_asked()[4:]  # past the first whole read's four asks
            self.assertEqual(after_first, ["PaneViewRequested"])
            self.assertEqual(ctx.asked[4][2]["provider"], "td.game")
            ctx, _ = self.watch([close(FILES)], out=os.path.join(self.root, "closed"))
            self.assertEqual(ctx.shapes_asked()[4:], ["DeskReadRequested"])
            ring = self.file("changes.txt", out=os.path.join(self.root, "closed"))
            self.assertIn("-- pane zengine.files/project-files: left the desk", ring)

        def test_a_notice_the_reading_stands_on_writes_nothing_and_asks_nothing(self):
            ctx, summary = self.watch([nothing, nothing])
            self.assertEqual(len(ctx.asked), 4)
            self.assertFalse(os.path.exists(os.path.join(self.out, "changes.txt")))
            self.assertIn("no change written", summary)
            self.assertIn("2 notice(s) that moved nothing it writes", summary)

        def test_a_stamp_moved_with_no_line_writes_no_change_and_keeps_the_files_current(self):
            ctx, summary = self.watch([caret(GAME)])
            self.assertFalse(os.path.exists(os.path.join(self.out, "changes.txt")))
            self.assertIn("no change written", summary)
            self.assertIn("1 notice(s) that moved nothing it writes", summary)
            # ...THE PANE READ AGAIN ALONE, its file naming the stamp it now stands on.
            self.assertEqual(ctx.shapes_asked()[4:], ["PaneViewRequested"])
            self.assertIn("stamp=7/1/0/%016x" % 1501,
                          [l for l in self.file("desk.txt").splitlines() if "td.game/board" in l][0])

        def test_two_moves_told_together_reach_the_reader_in_the_newest(self):
            ctx, _ = self.watch([twice])
            ring = self.file("changes.txt")
            self.assertEqual(len(self.entries()), 1)
            self.assertTrue(self.entries()[0].startswith("== change 1 -- notice 2: "), ring)
            self.assertIn("-- pane zengine.info/info", ring)
            self.assertIn("-- pane td.game/board", ring)

        def test_a_change_is_the_smaller_of_its_changed_lines_and_its_lines_as_they_stand(self):
            long_pane = ["row %d of a long list" % i for i in range(40)]
            desk = Desk()
            desk.pane(FILES).show(long_pane)
            edited = list(long_pane)
            edited[17] = "row 17 renamed"
            self.watch([words_of(FILES, edited), words_of(INFO, ["all new", "every line"])],
                       desk=desk)
            ring = self.file("changes.txt")
            self.assertIn("-- pane zengine.files/project-files: changed (- gone, + come)", ring)
            files_lines = ring.split("-- pane zengine.files/project-files")[1].splitlines()[1:]
            self.assertEqual([l for l in files_lines if l.startswith(("+ ", "- "))
                              and "row 17" in l], ["- 1003,375 row 17 of a long list",
                                                   "+ 1003,375 row 17 renamed"])
            self.assertIn("-- pane zengine.info/info: as it now stands", ring)

        def test_the_ring_keeps_the_last_64_changes_within_64_KiB_and_says_what_it_dropped(self):
            script = [words_of(INFO, ["PANES -- 3", "count %d" % i]) for i in range(70)]
            self.watch(script)
            entries = self.entries()
            self.assertEqual(len(entries), changes.RING_CHANGES)
            self.assertTrue(entries[0].startswith("== change 7 "), entries[0])
            self.assertTrue(entries[-1].startswith("== change 70 "), entries[-1])
            lines = self.file("changes.txt").splitlines()
            self.assertEqual(lines[0], changes.RING_HEAD)
            self.assertEqual(lines[1], "(6 earlier change(s) dropped)")
            self.assertLessEqual(len(self.file("changes.txt").encode("utf-8")), changes.RING_BYTES)
            # ...CONTINUED BY THE NEXT WATCH, numbered on, opening with what it does not say.
            self.watch([words_of(INFO, ["PANES -- 3", "next watch"])])
            self.assertEqual(self.entries()[-2], "== change 71 -- a new watch: what moved while "
                             "nothing watched is not said; this watch compares with its own first "
                             "reading")
            self.assertTrue(self.entries()[-1].startswith("== change 72 -- notice 1"))
            self.assertEqual(len(self.entries()), changes.RING_CHANGES)

        def test_a_change_past_the_ring_alone_is_cut_and_says_so(self):
            huge = ["%04d %s" % (i, "x" * 90) for i in range(900)]
            self.watch([words_of(FILES, huge)])
            ring = self.file("changes.txt")
            self.assertLessEqual(len(ring.encode("utf-8")), changes.RING_BYTES)
            self.assertIn("(cut: ", ring)
            self.assertIn("the reading files hold the desk as it now stands", ring)

        def test_a_ring_this_reader_did_not_write_starts_again_and_says_so(self):
            os.makedirs(self.out)
            with open(os.path.join(self.out, "changes.txt"), "w", encoding="utf-8") as f:
                f.write("someone else's notes\n== change 400 -- forged\n")
            self.watch([words_of(INFO, ["PANES -- 3", "fresh"])])
            ring = self.file("changes.txt")
            self.assertNotIn("forged", ring)
            self.assertIn("== change 1 -- the ring starts again: changes.txt was not one this "
                          "reader wrote", ring)
            self.assertTrue(self.entries()[-1].startswith("== change 2 -- notice"))

        def test_a_gap_reads_the_desk_again_whole_and_says_it(self):
            ctx, summary = self.watch([gap])
            self.assertEqual(ctx.shapes_asked()[4:], ["DeskReadRequested"])
            self.assertIn("after a gap of 1 lost notice(s), the desk read again whole",
                          self.entries()[0])
            self.assertIn("3,87 after the gap", self.file("changes.txt"))
            self.assertIn("1 gap(s) losing 1 notice(s)", summary)

        def test_two_watches_of_different_panes_each_write_only_their_own(self):
            script = lambda: [words_of(INFO, ["PANES -- 3", "info moved"]),
                              words_of(GAME, ["wave 1", "enemy at 9"]), select(GAME)]
            info_out, game_out = os.path.join(self.root, "info"), os.path.join(self.root, "game")
            self.watch(script(), out=info_out, panes=json.dumps([list(INFO)]))
            self.watch(script(), out=game_out, panes=json.dumps([list(GAME)]))
            info_ring = self.file("changes.txt", out=info_out)
            game_ring = self.file("changes.txt", out=game_out)
            self.assertIn("zengine.info/info", info_ring)
            self.assertNotIn("td.game", info_ring)
            self.assertIn("td.game/board", game_ring)
            self.assertNotIn("zengine.info", game_ring)
            self.assertNotIn("the desk's own lines", info_ring + game_ring)
            # ...AND ONLY THEIR OWN FILES and the guide: the desk's are not theirs to keep.
            self.assertEqual(os.listdir(os.path.join(info_out, "panes")), ["zengine.info.info.txt"])
            self.assertEqual(os.listdir(os.path.join(game_out, "panes")), ["td.game.board.txt"])
            for out in (info_out, game_out):
                self.assertFalse(os.path.exists(os.path.join(out, "desk.txt")))
                self.assertTrue(os.path.exists(os.path.join(out, "guide.md")))
            # The selection moving reaches the game's watch as its own header, and not the other.
            self.assertIn("selected keys", game_ring)
            self.assertEqual(len(self.entries(out=info_out)), 1)

        def test_a_desk_move_rewrites_a_pane_whose_stamp_held(self):
            # A PANE MOVED, nothing it shows changed: its words stand somewhere else now.
            ctx, _ = self.watch([shift(INFO, 40)])
            self.assertEqual(ctx.shapes_asked()[4:], ["DeskReadRequested"])
            ring = self.file("changes.txt")
            self.assertIn("-- pane zengine.info/info", ring)
            self.assertIn("43,87 info line", ring)
            self.assertIn("43,87 info line", self.file("panes", "zengine.info.info.txt"))
            self.assertNotIn("\n3,87 info line", self.file("desk.txt"))

        def test_a_desk_read_refused_writes_nothing_and_is_asked_again_at_the_next_notice(self):
            _, summary = self.watch([refused_then(INFO, ["PANES -- 3", "after the refusal"]),
                                     nothing])
            self.assertIn("1 desk read(s) refused and asked again at the next notice", summary)
            entries = self.entries()
            self.assertEqual(len(entries), 1, self.file("changes.txt"))
            self.assertTrue(entries[0].startswith("== change 1 -- notice 2"))
            self.assertIn("3,87 after the refusal", self.file("changes.txt"))
            self.assertNotIn("desk not read", self.file("changes.txt"))

        def test_a_watch_of_some_panes_removes_the_whole_desks_files_it_does_not_keep(self):
            os.makedirs(os.path.join(self.out, "panes"))
            for name in ("desk.txt", "desk-2.txt", "glance.txt"):
                with open(os.path.join(self.out, name), "w", encoding="utf-8") as f:
                    f.write("an older reading\n")
            self.watch([words_of(GAME, ["wave 1", "enemy at 5"])], panes=json.dumps([list(GAME)]))
            self.assertEqual(sorted(n for n in os.listdir(self.out) if n.endswith(".txt")),
                             ["changes.txt"])

        def test_one_watch_a_directory(self):
            os.makedirs(self.out)
            with open(os.path.join(self.out, ".desk-watch"), "w", encoding="utf-8") as f:
                f.write("desk/watch run watch-7\n")
            with self.assertRaises(loom_tool.ToolFailed) as raised:
                self.watch([words_of(INFO, ["never read"])])
            self.assertIn("is held by another desk/watch, desk/watch run watch-7",
                          str(raised.exception))
            self.assertFalse(os.path.exists(os.path.join(self.out, "AGENTS.md")))
            # ...AND A WATCH THAT ENDED GIVES THE DIRECTORY BACK.
            os.remove(os.path.join(self.out, ".desk-watch"))
            self.watch([words_of(INFO, ["PANES -- 3", "read"])])
            self.assertFalse(os.path.exists(os.path.join(self.out, ".desk-watch")))
            self.watch([words_of(INFO, ["PANES -- 3", "read again"])])

        def test_desk_reads_are_paced_and_end_after_the_changes_asked(self):
            ctx, summary = self.watch([select(GAME), select(INFO), select(FILES)], changes=2)
            self.assertEqual(len(self.entries()), 2)
            self.assertIn("it ended as 2 change(s) were written", summary)
            self.assertTrue(self.clock.slept)
            self.assertTrue(all(0 < s <= watch.DESK_READ_GAP for s in self.clock.slept),
                            self.clock.slept)
            reads = [i for i, a in enumerate(ctx.asked) if a[0] == "DeskReadRequested"]
            self.assertEqual(len(reads), 3)  # the first whole read, then one each change

        def test_a_pane_named_as_a_change_opens_splits_no_change_in_the_ring(self):
            ring = watch.changes.Ring()
            ring.add("notice 1: desk 5", ["-- pane x/change 3: came onto the desk",
                                          "== change 3 -- x x/change 3 open front=0 at 0,0 1x1"])
            ring.add("notice 2: desk 6", ["-- pane x/change 3: left the desk"])
            again = watch.changes.Ring.parse(ring.text())
            self.assertEqual([n for n, _ in again.entries], [1, 2], ring.text())
            self.assertEqual(again.next_number(), 3)
            self.assertIn(" == change 3 -- x x/change 3 open", ring.text())

        def test_a_replacement_refused_for_a_moment_is_tried_again(self):
            os.makedirs(self.out)
            path = os.path.join(self.out, "desk.txt")
            real, refusals = os.replace, []

            def replace(src, dst):
                if len(refusals) < 2:
                    refusals.append(dst)
                    raise PermissionError(13, "held open by a reader", dst)
                return real(src, dst)
            with patch.object(watch.read.time, "sleep"):
                with patch.object(watch.read.os, "replace", side_effect=replace):
                    watch.read.write(path, "the desk")
            self.assertEqual(len(refusals), 2)
            self.assertEqual(self.file("desk.txt"), "the desk")
            self.assertEqual(os.listdir(self.out), ["desk.txt"])

        def test_a_write_a_reader_holding_the_file_refuses_is_written_at_the_next_change(self):
            # ON WINDOWS A READER HOLDING changes.txt OPEN REFUSES ITS REPLACEMENT: the watch goes
            # on, and its next change writes the ring whole, the refused change in it.
            real, refusals = os.replace, []

            def replace(src, dst):
                if os.path.basename(dst) == "changes.txt" and not refusals:
                    refusals.append(dst)
                    raise PermissionError(13, "held open by a reader", dst)
                return real(src, dst)
            with patch.object(watch.read, "REPLACE_RETRY", 0.0):
                with patch.object(watch.read.os, "replace", side_effect=replace):
                    _, summary = self.watch([select(GAME), select(INFO)])
            self.assertEqual(len(refusals), 1)
            self.assertEqual(len(self.entries()), 2, self.file("changes.txt"))
            self.assertIn("2 change(s) written to changes.txt", summary)
            self.assertIn("1 write(s) refused while a reader held a file open", summary)

        def test_the_subscription_ending_ends_the_watch_and_says_so(self):
            _, summary = self.watch([ended, words_of(INFO, ["never read"])])
            self.assertIn("the notice's subscription ended (revoked)", summary)
            self.assertNotIn("never read", self.file("desk.txt"))

        def test_a_row_the_relay_refuses_fails_in_words_with_agents_md_written(self):
            with self.assertRaises(loom_tool.ToolFailed) as raised:
                self.watch([], refuse="guest 'watcher' may not observe DeskStamps v1: it says "
                                      "when the desk's words or its picture moved")
            self.assertIn("refused: this row may not follow the desk", str(raised.exception))
            self.assertIn("observe` entry naming zengine.workshop DeskStamps 1",
                          str(raised.exception))
            self.assertEqual(sorted(os.listdir(self.out)), ["AGENTS.md", "panes"])  # unheld
            self.assertEqual(os.listdir(os.path.join(self.out, "panes")), [])
            self.assertEqual(len(raised.exception.args), 1)

        def test_panes_not_a_list_of_pairs_is_refused_before_anything(self):
            with self.assertRaises(loom_tool.ToolFailed) as raised:
                self.watch([], panes='["zengine.info/info"]')
            self.assertIn("a JSON list of", str(raised.exception))
            self.assertFalse(os.path.exists(self.out))

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DeskWatchChecks)
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite).wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True)
    parser.add_argument("--runtime", required=True)
    args = parser.parse_args()
    sys.exit(0 if run_checks(args.tools, args.runtime) else 1)
