# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Verdict checks for the Workshop tools that wait on an owner: `workshop/builder`'s completion and
`workshop/nvim-edit`'s confirmation, run through their real entry points against scripted panes.

Run by workshop_journey before its real processes (check T2). A scripted pane is the rows a
PaneView would carry, so these checks pin what each tool CONCLUDES from an owner's words --
delayed and immediate outcomes, unchanged or stale rows, refusals, failures, pending work and
success -- and what it refuses to send. They do not claim that a real Builder or Neovim painted
those rows: the Neovim route is tests/session/neovim_journey.py's, on a real Neovim.
Standalone: python workshop_verdict_checks.py --tools <package> --runtime <loom runtime>
"""

import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace
import unittest

LETTERS = dict((chr(ord("a") + i), 4 + i) for i in range(26))


class CheckFailed(Exception):
    pass


class Clock:
    """Time for a tool under test: sleeping moves it, nothing waits."""

    def __init__(self):
        self.now = 1000.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += max(seconds, 0.01)


class Scripted:
    """A run context whose Workshop is a script. `pane(provider, pane)` answers PaneView with rows
    (a list of texts) or None (not described); every injected moment goes to `typed`."""

    name = "verdict-check"

    def __init__(self, steps, pane, **inputs):
        self.inputs = dict(link="workshop", **inputs)
        self.steps, self.pane = steps, pane
        self.moments, self.produced, self.session_open = [], {}, False
        self.conn = SimpleNamespace(schema=self.schema)

    def schema(self, *args):
        t = self.steps
        fields = [("kind", t.TEXT), ("scancode", t.INT), ("modifiers", t.INT), ("text", t.TEXT),
                  ("button", t.INT), ("pressed", t.BOOL), ("x", t.INT), ("y", t.INT),
                  ("space", t.INT), ("dx", t.INT), ("dy", t.INT)]
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

    def produce(self, name, data):
        self.produced[name] = data

    def on_cleanup(self, callback, label):
        pass

    def ask(self, office, shape, fields, **options):
        if shape == "InputSessionRequested":
            self.session_open = True
            return {"session": 1}
        if shape == "InputSessionClosed":
            self.session_open = False
            return {}
        if shape == "InjectInput":
            for m in fields["events"]:
                self.moments.append(m)
                self.typed(m)
            return {"session": 1, "admitted": len(fields["events"])}
        if shape == "PaneViewRequested":
            rows = self.pane(fields["provider"], fields["pane"])
            if rows is None:
                from loom_session.tool import Refused
                raise Refused("not described")
            return {"picture": 1, "rows": [{"row": i, "text": t, "x": 1, "y": i, "space": 1}
                                           for i, t in enumerate(rows)]}
        if shape == "PanePointRequested":
            return {"x": 1, "y": fields["row"], "space": 1}
        raise AssertionError(shape)

    def typed(self, moment):
        pass

    def keys(self):
        """The chords pressed, as (scancode, modifiers), in order."""
        return [(m["scancode"], m["modifiers"]) for m in self.moments if m["kind"] == "KeyPressed"]


def run_checks(tools, runtime):
    sys.path[:0] = [str(Path(runtime).resolve()), str(Path(tools).resolve())]
    steps = importlib.import_module("workshop_steps")
    builder = importlib.import_module("builder")
    nvim_edit = importlib.import_module("nvim_edit")

    # ---- the Builder ---------------------------------------------------------------------------
    def rows(last, realize, asks, notice=None, project=None, armed=False):
        out = [notice] if notice else []
        out += ["BUILDER @zengine.builder  game/build-recipes.json",
                "recipe   tower-defense -> tower-defense  (1/2)"]
        out += ["project  " + project] if project else []
        out += ["last     " + last]
        # A short pane gives up the `exit` row first when a notice or the project row needs room.
        out += ["exit     --         asks %d ever" % asks] if asks is not None else []
        out += ["realize  " + realize, "said     --",
                "[menu] [choose a recipe...] [build] [turn load-after-build %s] [add to the load plan...]"
                % ("off" if armed else "on")]
        return out

    BEFORE = rows("succeeded -- op #7, 12 out", "realized -- op #7", 7)
    TRIGGERS = {"build": (LETTERS["b"], 0), "frontier": (LETTERS["f"], 0),
                "arm": (LETTERS["b"], 1), "load-built": (LETTERS["b"], 1),
                "promote": (LETTERS["p"], 1), "revert": (LETTERS["r"], 1), "load-it": (LETTERS["o"], 0)}

    class BuilderPane(Scripted):
        """frames[0] until the act's key is pressed; after it, one frame per read, the last kept."""

        def __init__(self, frames, act, **inputs):
            Scripted.__init__(self, steps, self.view, act=act, seconds=5, **inputs)
            self.frames, self.trigger, self.fired, self.reads = frames, TRIGGERS.get(act), False, 0

        def typed(self, m):
            if m["kind"] == "KeyPressed" and (m["scancode"], m["modifiers"]) == self.trigger:
                self.fired = True

        def view(self, provider, pane):
            if not self.fired:
                return self.frames[0]
            self.reads += 1
            return self.frames[min(self.reads, len(self.frames) - 1)]

    class BuilderChecks(unittest.TestCase):
        def run_builder(self, frames, act, **inputs):
            clock = Clock()
            builder.time = clock
            ctx = BuilderPane(frames, act, **inputs)
            try:
                said = builder.run(ctx)
                error = None
            except CheckFailed as err:
                said, error = None, str(err)
            record = json.loads(ctx.produced["builder.json"]) if "builder.json" in ctx.produced else None
            return SimpleNamespace(said=said, error=error, record=record, ctx=ctx, clock=clock)

        def test_frontier_waits_for_the_build_and_then_its_realization(self):
            frames = [BEFORE,
                      rows("succeeded -- op #7, 12 out", "realized -- op #7", 7),  # stale: not taken
                      rows("asked -- waiting for it to start", "asked -- op #0", 8),
                      rows("running -- op #8, 1 out", "asked -- op #8", 8),
                      rows("succeeded -- op #8, 4 out", "offered -- op #8 -- offered to the project", 8),
                      rows("succeeded -- op #8, 4 out", "realized -- op #8, NOT DEFAULT (promote / revert)"
                           " -- reloaded in place -- weave #37 keeps its id and its state", 8)]
            r = self.run_builder(frames, "frontier")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["build"], {"op": 8, "outcome": "succeeded"})
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            self.assertEqual(r.record["realization"]["op"], 8)
            self.assertIn("op #8 build succeeded; realization realized", r.said)

        def test_an_outcome_already_there_at_the_first_read_is_accepted(self):
            frames = [BEFORE, rows("succeeded -- op #8, 4 out", "realized -- op #8", 8)]
            r = self.run_builder(frames, "frontier")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["realization"]["outcome"], "realized")

        def test_a_pending_realization_stays_unresolved_with_its_operation(self):
            frames = [BEFORE, rows("succeeded -- op #8, 4 out", "offered -- op #8 -- offered to the project", 8)]
            r = self.run_builder(frames, "frontier")
            self.assertRegex(r.error, r"^UNRESOLVED: op #8's build ended succeeded and its "
                                      r"realization is pending .* `act=look op=8`")
            self.assertEqual(r.record["unresolved"]["op"], 8)
            self.assertEqual(r.record["build"], {"op": 8, "outcome": "succeeded"})

        def test_expect_any_does_not_turn_a_running_build_into_an_ending(self):
            frames = [BEFORE, rows("running -- op #8, 1 out", "asked -- op #8", 8)]
            r = self.run_builder(frames, "frontier", expect="any")
            self.assertRegex(r.error, r"^UNRESOLVED: the build has not ended")
            self.assertIsNone(r.record["build"])

        def test_unchanged_rows_are_not_a_taken_ask(self):
            r = self.run_builder([BEFORE, BEFORE], "build")
            self.assertRegex(r.error, r"^UNRESOLVED: the Builder did not take this ask")
            self.assertLessEqual(r.clock.now - 1000.0, builder.TAKE_SECONDS + 1)

        def test_a_refusal_is_the_panes_own_words_and_ends_the_wait(self):
            refused = rows("succeeded -- op #7, 12 out", "realized -- op #7", 7,
                           notice="this project is not waiting on any artifact -- nothing was asked for")
            r = self.run_builder([BEFORE, refused], "frontier")
            self.assertRegex(r.error, r"^the Builder asked for nothing: this project is not waiting")
            self.assertLess(r.clock.now - 1000.0, 1.0)

        def test_without_the_asks_row_a_new_operation_number_names_the_ask(self):
            first = rows("not built yet", "-- (load-after-build arms it)", None,
                         notice="loaded `tower-defense` as td.game -- pending",
                         project="waiting tower-defense (tower-defense, blocks 0)")
            frames = [first,
                      rows("asked -- waiting for it to start", "asked -- op #0", None, notice="asked the "
                           "Builder for `tower-defense` and to realize it -- Workshop stays live"),
                      rows("FAILED -- op #1, 9 out -- read output", "REFUSED -- op #1 -- the build failed, "
                           "so nothing was offered to the project", None, notice="FAILED `tower-defense`")]
            r = self.run_builder(frames, "frontier", expect="any")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["ask"]["attributed_by"], "a new operation number")
            self.assertEqual(r.record["build"], {"op": 1, "outcome": "FAILED"})

        def test_an_older_operation_ending_is_not_this_ask(self):
            busy = rows("running -- op #7, 3 out", "asked -- op #7", None, notice="asked the Builder "
                        "for `tower-defense` -- Workshop stays live")
            done = rows("succeeded -- op #7, 9 out", "realized -- op #7", None, notice="succeeded")
            r = self.run_builder([busy, done], "build")
            self.assertRegex(r.error, r"^UNRESOLVED: the Builder did not take this ask")
            self.assertIsNone(r.record["build"])

        def test_two_asks_taken_cannot_be_told_apart(self):
            r = self.run_builder([BEFORE, rows("running -- op #9, 1 out", "asked -- op #9", 9)], "build")
            self.assertIn("cannot tell which one is its own", r.error)

        def test_a_failed_build_is_judged_by_expect_and_its_realization_reported_apart(self):
            failed = rows("FAILED -- op #8, 9 out -- read output",
                          "REFUSED -- op #8 -- the build failed, so nothing was offered to the project", 8)
            for expect, passes in (("succeeded", False), ("failed", True), ("any", True)):
                with self.subTest(expect=expect):
                    r = self.run_builder([BEFORE, failed], "frontier", expect=expect)
                    self.assertEqual(r.error is None, passes, r.error)
                    self.assertEqual(r.record["build"]["outcome"], "FAILED")
                    self.assertEqual(r.record["realization"]["outcome"], "REFUSED")
                    self.assertIn("output.txt", r.ctx.produced)

        def test_realize_judges_the_realization_alone(self):
            refused = rows("succeeded -- op #8, 4 out", "REFUSED -- op #8 -- the rebuilt weave keeps a "
                           "different STATE", 8)
            self.assertIn("not realized", self.run_builder([BEFORE, refused], "frontier",
                                                           realize="realized").error)
            self.assertIsNone(self.run_builder([BEFORE, refused], "frontier", realize="refused").error)
            self.assertIsNone(self.run_builder([BEFORE, refused], "frontier").error)

        def test_a_plain_build_asked_nothing_of_realization(self):
            plain = rows("succeeded -- op #8, 4 out", "-- (load-after-build loads tower-defense now)", 8)
            r = self.run_builder([BEFORE, plain], "build")
            self.assertIsNone(r.error)
            self.assertFalse(r.record["realization"]["asked"])
            self.assertIn("not asked", self.run_builder([BEFORE, plain], "build", realize="realized").error)

        def test_arm_confirms_the_switch_and_never_toggles_it_blind(self):
            armed = rows("succeeded -- op #7, 12 out", "realized -- op #7", 7, armed=True,
                         notice="load after build: on -- the next build is offered to the running project")
            r = self.run_builder([BEFORE, armed], "arm")
            self.assertIsNone(r.error)
            self.assertEqual(r.ctx.keys().count(TRIGGERS["arm"]), 1)
            r = self.run_builder([armed], "arm")
            self.assertIsNone(r.error)
            self.assertNotIn(TRIGGERS["arm"], r.ctx.keys())
            r = self.run_builder([BEFORE, BEFORE], "arm")
            self.assertRegex(r.error, r"^UNRESOLVED: the load-after-build switch did not answer")
            waiting = rows("succeeded -- op #7, 12 out", "-- (load-after-build loads tower-defense now)", 7)
            r = self.run_builder([waiting], "arm")
            self.assertIn("Nothing was pressed", r.error)
            self.assertNotIn(TRIGGERS["arm"], r.ctx.keys())

        def test_load_built_needs_a_built_product_waiting(self):
            r = self.run_builder([BEFORE], "load-built")
            self.assertIn("Nothing was pressed", r.error)
            self.assertNotIn(TRIGGERS["load-built"], r.ctx.keys())

        def test_promote_waits_for_the_owners_answer(self):
            standing = rows("succeeded -- op #8, 4 out", "realized -- op #8, NOT DEFAULT (promote / revert) "
                            "-- reloaded in place -- weave #37 keeps its id and its state", 8)
            promoted = rows("succeeded -- op #8, 4 out", "realized -- op #8 -- promoted: the next launch "
                            "runs the image weave #37 is running now", 8)
            refused = rows("succeeded -- op #8, 4 out", "realized -- op #8, NOT DEFAULT (promote / revert) "
                           "-- the running 'tower-defense' could not be promoted: denied", 8)
            self.assertIsNone(self.run_builder([standing, standing, promoted], "promote").error)
            self.assertIn("could not be promoted", self.run_builder([standing, refused], "promote").error)
            self.assertRegex(self.run_builder([standing, standing], "promote").error, r"^UNRESOLVED: no answer")
            r = self.run_builder([rows("not built yet", "-- (load-after-build arms it)", 0)], "promote")
            self.assertIn("nothing was pressed", r.error)

        def test_load_it_is_confirmed_by_the_plan_rows_sentence(self):
            line = ["load `tower-defense` -- type the role it holds", "role for tower-defense> ",
                    "loads    tower-defense (built by `tower-defense`)"]
            pending = rows("not built yet", "-- (load-after-build arms it)", 0,
                           project="waiting tower-defense (tower-defense, blocks 1)",
                           notice="loaded `tower-defense` as td.game -- pending -- the project is waiting on it")
            first = rows("not built yet", "-- (load-after-build arms it)", 0)
            r = self.run_builder([first, line, line, pending], "load-it", role="td.game")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["realization"]["outcome"], "pending")
            self.assertIn("not realized", self.run_builder([first, line, pending], "load-it",
                                                           role="td.game", realize="realized").error)
            again = rows("not built yet", "-- (load-after-build arms it)", 0,
                         notice="`tower-defense` is already in this project's plan -- build-and-load it instead")
            self.assertIn("already in this project's plan",
                          self.run_builder([first, again], "load-it", role="td.game").error)
            self.assertRegex(self.run_builder([first, first], "load-it", role="td.game").error,
                             r"^UNRESOLVED: `o` opened no role line")
            loading = rows("asked -- waiting for it to start", "asked -- op #0", 1,
                           notice="loading `tower-defense` now -- loaded as td.game, pending; Workshop stays live")
            done = rows("succeeded -- op #3, 2 out", "realized -- op #3", 1)
            r = self.run_builder([first, line, loading, done], "load-it", role="td.game", realize="realized")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["build"], {"op": 3, "outcome": "succeeded"})

        # ---- whose operation it is, to the end (`builder.Mine`) ----------------------------------
        def test_a_later_build_is_never_taken_for_the_acknowledged_one(self):
            # The review's controlled sequence: the counter took this run's ask (8) beside op #8,
            # then the next read shows op #9 ended -- a later build, accepted after #8 ended unseen.
            frames = [BEFORE, rows("running -- op #8, 1 out", "asked -- op #8", 8),
                      rows("succeeded -- op #9, 4 out", "realized -- op #9", 9)]
            r = self.run_builder(frames, "frontier")
            self.assertRegex(r.error, r"^SUPERSEDED .*another ask after this run's \(asks 9 ever; "
                                      r"this run's is ask 8\)")
            self.assertIsNone(r.record["build"])
            self.assertEqual((r.record["superseded"]["op"], r.record["superseded"]["ask"]), (8, 8))
            self.assertEqual((r.record["ask"]["number"], r.record["ask"]["op"]), (8, 8))

        def test_an_acknowledged_build_still_running_keeps_its_operation_and_route(self):
            r = self.run_builder([BEFORE, rows("running -- op #8, 1 out", "asked -- op #8", 8)],
                                 "frontier", expect="any")
            self.assertRegex(r.error, r"^UNRESOLVED: the build has not ended .* `act=look op=8`")
            self.assertEqual((r.record["unresolved"]["op"], r.record["unresolved"]["ask"]), (8, 8))

        def test_an_ask_taken_before_its_operation_is_bound_when_one_is_numbered(self):
            asked = rows("asked -- waiting for it to start", "asked -- op #0", 8)
            running = rows("running -- op #8, 1 out", "asked -- op #8", 8)
            done = rows("succeeded -- op #8, 4 out", "realized -- op #8", 8)
            r = self.run_builder([BEFORE, asked, running], "frontier")
            self.assertRegex(r.error, r"^UNRESOLVED: the build has not ended .* `act=look op=8`")
            self.assertEqual(r.record["unresolved"]["op"], 8)
            # THE OPERATION STANDING BEFORE THE PRESS is never bound to this ask, beside any count.
            r = self.run_builder([BEFORE, rows("succeeded -- op #7, 12 out", "realized -- op #7", 8)],
                                 "frontier")
            self.assertRegex(r.error, r"^UNRESOLVED: the build has not ended")
            self.assertIsNone(r.record["build"])
            r = self.run_builder([BEFORE, asked, running, done], "frontier")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["build"], {"op": 8, "outcome": "succeeded"})
            self.assertEqual(r.record["ask"]["op_named_by"], "the asks counter beside it")
            # AN ASK NEVER NUMBERED STAYS UNKNOWN: no operation is invented for the record.
            r = self.run_builder([BEFORE, asked], "frontier")
            self.assertRegex(r.error, r"^UNRESOLVED: the build has not ended")
            self.assertNotIn("act=look", r.error)
            self.assertEqual((r.record["unresolved"]["op"], r.record["unresolved"]["ask"]), (None, 8))

        def test_a_request_taken_before_this_ask_was_numbered_is_not_its_operation(self):
            frames = [BEFORE, rows("asked -- waiting for it to start", "asked -- op #0", 8),
                      rows("running -- op #9, 1 out", "asked -- op #9", 9),
                      rows("succeeded -- op #9, 4 out", "realized -- op #9", 9)]
            r = self.run_builder(frames, "frontier")
            self.assertRegex(r.error, r"^SUPERSEDED .*this run's is ask 8.*was not seen to become "
                                      r"an operation")
            self.assertEqual((r.record["superseded"]["op"], r.record["superseded"]["ask"]), (None, 8))

        def test_without_the_counter_the_first_new_operation_is_kept_to_the_end(self):
            hidden = dict(notice="asked the Builder for `tower-defense` -- Workshop stays live")
            running = rows("running -- op #8, 1 out", "asked -- op #8", None, **hidden)
            r = self.run_builder([BEFORE, running, rows("succeeded -- op #9, 4 out", "realized -- op #9",
                                                        None, **hidden)], "frontier")
            self.assertRegex(r.error, r"op #8 is no longer the Builder's latest \(it shows op #9\)")
            # A counter painted again beside op #8 two past the 7 standing: which ask was this run's
            # cannot be told, so the operation named by number alone is not taken either.
            r = self.run_builder([BEFORE, running, rows("succeeded -- op #8, 4 out", "realized -- op #8",
                                                        9)], "frontier")
            self.assertRegex(r.error, r"^SUPERSEDED .*cannot be told")
            # ...and the counter one past it names this run's ask, kept in the record.
            done = rows("succeeded -- op #8, 4 out", "realized -- op #8", 8)
            r = self.run_builder([BEFORE, running, done], "frontier")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["ask"]["attributed_by"], "a new operation number")
            self.assertEqual((r.record["ask"]["number"], r.record["build"]["op"]), (8, 8))

        def test_a_realization_waited_for_stays_this_operations(self):
            frames = [BEFORE, rows("running -- op #8, 1 out", "asked -- op #8", 8),
                      rows("succeeded -- op #8, 4 out", "offered -- op #8 -- offered to the project", 8),
                      rows("asked -- waiting for it to start", "asked -- op #0", 9),
                      rows("succeeded -- op #9, 4 out", "realized -- op #9", 9)]
            r = self.run_builder(frames, "frontier")
            self.assertRegex(r.error, r"^SUPERSEDED \(op #8's build ended succeeded and its "
                                      r"realization is pending\)")
            self.assertEqual(r.record["build"], {"op": 8, "outcome": "succeeded"})
            self.assertNotIn("outcome", r.record["realization"])
            self.assertEqual(r.record["superseded"]["op"], 8)

        def test_a_later_asks_ending_that_names_no_operation_is_not_this_ones(self):
            plain = "-- (load-after-build arms it)"
            frames = [BEFORE, rows("running -- op #8, 1 out", plain, 8),
                      rows("did not start", plain, None, notice="did not start `tower-defense`")]
            for expect in ("failed", "any"):
                with self.subTest(expect=expect):
                    r = self.run_builder(frames, "build", expect=expect)
                    self.assertRegex(r.error, r"^SUPERSEDED .*op #8 is no longer the Builder's latest")
                    self.assertIsNone(r.record["build"])
            # An ask that itself never became a process still ends there, with no operation.
            never = rows("did not start", plain, 8, notice="did not start `tower-defense`")
            r = self.run_builder([BEFORE, never], "build", expect="failed")
            self.assertIsNone(r.error)
            self.assertEqual(r.record["build"], {"op": 0, "outcome": "did not start"})

        def test_look_follows_one_operation_and_presses_nothing(self):
            running = rows("running -- op #8, 1 out", "asked -- op #8", 8)
            done = rows("succeeded -- op #8, 4 out", "realized -- op #8", 8)
            ctx_frames = [running, running, done]
            pane = BuilderPane(ctx_frames, "look", op=8)
            pane.fired = True
            builder.time = Clock()
            self.assertIn("op #8 build succeeded", builder.run(pane))
            self.assertEqual(pane.keys(), [])
            later = rows("running -- op #9, 1 out", "asked -- op #9", 9)
            pane = BuilderPane([later], "look", op=8)
            with self.assertRaisesRegex(CheckFailed, "no longer the Builder's latest"):
                builder.run(pane)

    # ---- nvim-edit ------------------------------------------------------------------------------
    class NeovimPane(Scripted):
        """Workshop's Neovim pane over a small model of Neovim: its current buffer, the buffers it
        holds (path -> [lines, modified]), 'hidden', its mode and last row. It answers what the
        tool types: `:e`, the lone `:`, the `let`/`echo` question, Escape and `:w`."""

        def __init__(self, disk, current, buffers, hidden=True, prompt_after_e=False,
                     save_leaves_modified=False, **inputs):
            Scripted.__init__(self, steps, self.view, **inputs)
            self.disk, self.current, self.buffers = disk, current, buffers
            self.hidden, self.prompt_after_e = hidden, prompt_after_e
            self.save_leaves_modified = save_leaves_modified
            self.mode, self.last, self.texts = "NORMAL", "", []

        def status(self):
            modified = self.buffers.get(self.current, [[], False])[1]
            return "%s %s clean -- %s" % ("UNSAVED" if modified else "saved", self.mode, self.current)

        def view(self, provider, pane):
            lines = self.buffers.get(self.current, [[], False])[0]
            return [self.status()] + (lines or [""]) + ["~", self.current, self.last]

        def typed(self, m):
            if m["kind"] == "KeyPressed" and m["scancode"] == 41 and self.mode != "PROMPT":
                self.mode = "NORMAL"
            if m["kind"] != "TextEntered":
                return
            text = m["text"]
            self.texts.append(text)
            if self.mode == "PROMPT":
                return  # a prompt takes none of this
            if text.startswith(":e "):
                path = text[3:].strip()
                cur = self.buffers.get(self.current)
                if cur and cur[1] and (path == self.current or not self.hidden):
                    self.last = "E37: No write since last change (add ! to override)"
                    return
                if self.prompt_after_e:
                    self.mode, self.last = "PROMPT", "[O]pen Read-Only, (E)dit anyway, (Q)uit, (A)bort:"
                    return
                self.buffers.setdefault(path, [list(self.disk.get(path, [])), False])
                self.current = path
            elif text == ":":
                self.mode, self.last = "COMMAND", ":"
            elif text.startswith("let g:nvim_edit="):
                token = re.search(r"'nvim-edit' '([0-9a-f]+)'", text).group(1)
                wanted = re.search(r"==[?#]'([^']*)'", text).group(1)
                lines, modified = self.buffers.get(self.current, [[], False])
                digest = hashlib.sha256(("\n".join(lines or [""]) + "\n").encode()).hexdigest()[:16]
                self.mode = "NORMAL"
                self.last = "nvim-edit %s %d %d %d %s" % (token, self.current == wanted, modified,
                                                          max(1, len(lines)), digest)
            elif text.startswith(":w"):
                self.disk_write()
                self.mode = "NORMAL"

        def disk_write(self):
            lines, _ = self.buffers[self.current]
            Path(self.current).write_bytes(("\n".join(lines) + "\n" if lines else "").encode("utf-8"))
            self.buffers[self.current][1] = self.save_leaves_modified

    class NeovimChecks(unittest.TestCase):
        def setUp(self):
            self.dir = tempfile.TemporaryDirectory(prefix="zengine-nvim-edit-check-")
            self.root = Path(self.dir.name).resolve()
            nvim_edit.time = Clock()

        def tearDown(self):
            self.dir.cleanup()

        def path(self, name):
            return Path(os.path.normpath(str(self.root / name))).as_posix()

        def edit(self, pane, path, edits):
            pane.inputs.update(path=path, edits=json.dumps(edits))
            try:
                return nvim_edit.run(pane), None
            except CheckFailed as err:
                return None, str(err)

        def after_ask(self, pane):
            """Everything typed after the first answer to the question."""
            at = [i for i, t in enumerate(pane.texts) if t.startswith("let g:nvim_edit=")]
            return pane.texts[at[0] + 1:] if at else None

        def test_a_draft_never_saved_is_refused_and_kept(self):
            target = self.path("draft.txt")
            pane = NeovimPane({}, target, {target: [["MY DRAFT"], True]})
            said, error = self.edit(pane, target, [{"create": "replacement\n"}])
            self.assertIn("a document never saved", error)
            self.assertEqual(pane.buffers[target], [["MY DRAFT"], True])
            self.assertEqual(self.after_ask(pane), [])
            self.assertFalse(Path(target).exists())
            record = json.loads(pane.produced["edit.json"])
            self.assertEqual((record["verdict"], record["opened"]["modified"]), ("UNSAVED", True))

        def test_an_unsaved_change_to_a_file_on_disk_is_refused(self):
            target = self.path("notes.txt")
            Path(target).write_bytes(b"one\n")
            pane = NeovimPane({target: ["one"]}, target, {target: [["one", "UNSAVED"], True]})
            said, error = self.edit(pane, target, [{"append": "two\n"}])
            self.assertIn("Neovim holds unsaved changes to", error)
            self.assertEqual(Path(target).read_bytes(), b"one\n")
            self.assertEqual(self.after_ask(pane), [])

        def test_an_open_neovim_refused_is_not_read_from_the_other_buffer(self):
            a, b = self.path("a1/abcdefghijklmnopqrstuvwxyz.txt"), self.path("a2/abcdefghijklmnopqrstuvwxyz.txt")
            self.assertEqual(a[-24:], b[-24:])
            pane = NeovimPane({}, a, {a: [["DRAFT IN A"], True]}, hidden=False)
            said, error = self.edit(pane, b, [{"create": "text for B\n"}])
            self.assertIn("did not open", error)
            self.assertEqual(pane.buffers[a], [["DRAFT IN A"], True])
            self.assertEqual(self.after_ask(pane), [])
            self.assertFalse(Path(a).exists() or Path(b).exists())

        def test_an_unmodified_buffer_that_is_not_the_file_is_refused(self):
            target = self.path("gone.txt")
            pane = NeovimPane({}, target, {target: [["text read before the file was removed"], False]})
            said, error = self.edit(pane, target, [{"create": "new\n"}])
            self.assertIn("is not the file on disk", error)
            self.assertEqual(self.after_ask(pane), [])

        def test_a_prompt_after_open_ends_the_run_before_the_question(self):
            target = self.path("swapped.txt")
            pane = NeovimPane({}, self.path("other.txt"), {}, prompt_after_e=True)
            said, error = self.edit(pane, target, [{"create": "x\n"}])
            self.assertIn("waiting at a prompt", error)
            self.assertFalse(any(t.startswith("let ") for t in pane.texts))

        def test_success_needs_the_buffer_saved_as_well_as_the_bytes(self):
            target = self.path("saved.txt")
            pane = NeovimPane({}, self.path("other.txt"), {}, save_leaves_modified=True)
            # The model types nothing into buffers; the plan's text is what `:w` writes.
            original = pane.typed

            def typed(m):
                if m["kind"] == "TextEntered" and m["text"] == "planned" and pane.current == target:
                    pane.buffers[target][0] = ["planned"]
                    pane.buffers[target][1] = True
                original(m)
            pane.typed = typed
            said, error = self.edit(pane, target, [{"create": "planned\n"}])
            self.assertEqual(Path(target).read_bytes(), b"planned\n")
            self.assertIn("did not save", error)
            record = json.loads(pane.produced["edit.json"])
            self.assertEqual(record["verdict"], "NOT SAVED")
            pane.save_leaves_modified = False
            Path(target).unlink()
            pane.buffers.pop(target)
            said, error = self.edit(pane, target, [{"create": "planned\n"}])
            self.assertIsNone(error)
            self.assertEqual(json.loads(pane.produced["edit.json"])["verdict"], "matches")

    suite = unittest.TestSuite()
    for case in (BuilderChecks, NeovimChecks):
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(case))
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite).wasSuccessful()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True)
    parser.add_argument("--runtime", required=True)
    args = parser.parse_args()
    sys.exit(0 if run_checks(args.tools, args.runtime) else 1)
