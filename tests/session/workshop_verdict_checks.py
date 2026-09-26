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
    # `workshop/builder` presses the pane's keys and follows the Builder's OWN typed words
    # (BuildAsked, BuildStatus) through a subscription made before the first key. A script here is
    # what that subscription hands over: words the press set in motion (marked with the press's
    # own correlation, as a settled press delivers them) and words that arrive later, one per
    # wait. The pane still answers what the pane owns: its rows, its notice, its switch.
    from loom_session import observe as lobserve

    def rows(notice=None, realize="realized -- op #7", armed=False,
             recipe="tower-defense -> tower-defense  (1/2)", last="succeeded -- op #7, 12 out"):
        out = [notice] if notice else []
        out += ["BUILDER @zengine.builder  game/build-recipes.json", "recipe   " + recipe,
                "last     " + last, "exit     --         asks 7 ever", "realize  " + realize,
                "said     --",
                "[menu] [choose a recipe...] [build] [turn load-after-build %s] [add to the load plan...]"
                % ("off" if armed else "on")]
        return out

    BEFORE = rows()

    def status(builds, op, outcome, realization=0, realize=False, detail="", realized="",
               default=False):
        return ("BuildStatus", {"recipe": "tower-defense", "artifact": "tower-defense",
                                "outcome": outcome, "status": 2 if outcome == 3 else 0,
                                "command": "cmake --build", "detail": detail, "builds": builds,
                                "op": op, "chunks": 0, "realize": realize,
                                "realization": realization, "realized_detail": realized,
                                "default_image": default})

    def asked(number, taken=True, refusal="", realize=False):
        return ("BuildAsked", {"ask": number, "recipe": "tower-defense", "realize": realize,
                               "taken": taken, "refusal": refusal})

    # The realization owner's words (`zengine.realization`): what it did with an ask, and the
    # answers naming it.
    def owner_asked(number, act, taken=True, refusal=""):
        return ("RealizationAsked", {"artifact": "tower-defense", "act": act, "ask": number,
                                     "taken": taken, "refusal": refusal})

    def promoted(number, ok=True, detail="promoted: the next launch runs the image weave #37"):
        return ("ArtifactPromoted", {"artifact": "tower-defense", "promoted": ok,
                                     "detail": detail, "ask": number})

    def realized(number, ok=True, detail="reverted: weave #37 runs the image before its reload",
                 default=False):
        return ("ArtifactRealized", {"artifact": "tower-defense", "realized": ok,
                                     "detail": detail, "default_image": default, "ask": number})

    def other(word):
        """A word some OTHER press set in motion."""
        return ("other", word)

    def by(word, producer):
        """A word said by another holder of the office: a replaced owner, counting afresh."""
        return ("by", word, producer)

    GAP, LOST = ("gap", 3), ("ended", "lost")
    TRIGGERS = {"build": (LETTERS["b"], 0), "frontier": (LETTERS["f"], 0),
                "arm": (LETTERS["b"], 1), "load-built": (LETTERS["b"], 1),
                "promote": (LETTERS["p"], 1), "revert": (LETTERS["r"], 1), "load-it": (LETTERS["o"], 0)}
    # The press whose settled work carries the Builder's words: the act's key, or for load-it the
    # Return that submits the role (40).
    CAUSE_KEYS = {"load-it": (40, 0)}

    class Said:
        """What the subscription hands over. `caused` arrive with the act's press; `later`, one per
        wait; a wait with nothing left moves the clock by its whole timeout."""

        def __init__(self, clock, caused, later):
            self.clock, self.caused, self.later = clock, list(caused), list(later)
            self.ready, self.seq, self.order = [], 0, []
            self.subscription, self.relay, self.holder, self.incarnation = 1, "R", 5, 1
            self.window = 256

        def word(self, spec, cause, producer=5):
            kind = spec[0]
            if kind == "other":
                return self.word(spec[1], 999, producer)
            if kind == "by":
                return self.word(spec[1], cause, spec[2])
            self.seq += 1
            if kind == "gap":
                return lobserve.Gap(self.seq, spec[1], "the window was shut", local=False)
            if kind == "ended":
                return lobserve.Ended({"seq": self.seq, "kind": spec[1], "reason": "scripted"})
            return lobserve.Observation(kind, 1, dict(spec[1]), {
                "seq": self.seq, "cause": cause, "delivery": 100 + self.seq,
                "published_in": 50 + self.seq, "producer": producer, "incarnation": 1},
                self.clock.now)

        def press(self, correlation):
            for spec in self.caused:
                self.ready.append(self.word(spec, correlation))
            self.caused = []

        def drain(self):
            out, self.ready = self.ready, []
            return out

        def next(self, timeout):
            if self.ready:
                return self.ready.pop(0)
            if self.later:
                return self.word(self.later.pop(0), 0)
            self.clock.sleep(timeout or 0.01)
            return None

        def summary(self):
            return {"subscription": 1, "scripted": True}

    class PressAnswer(dict):
        correlation = 0

    class BuilderScript(Scripted):
        """The pane shows frames[0] until the act's key is pressed and then one frame per read
        (the last kept); the Builder says `caused` with that press and `later` afterwards, and the
        realization owner `owner_caused` and `owner_later`. `baseline` is the Builder's answer to
        `BuildStatusRequested` (None: the ask is refused), and `meanwhile` its words published
        between the subscription and that answer. `owner_refused` refuses the owner's subscription.
        Every shape asked is kept (`asked_shapes`), so a check can say what a run never asked."""

        def __init__(self, act, clock, caused=(), later=(), frames=(BEFORE,), baseline=None,
                     meanwhile=(), owner_caused=(), owner_later=(), owner_refused=False, **inputs):
            Scripted.__init__(self, steps, self.view, act=act, seconds=5, **inputs)
            self.frames, self.trigger, self.fired, self.reads = list(frames), TRIGGERS.get(act), False, 0
            self.cause_key, self.cause_fired = CAUSE_KEYS.get(act, self.trigger), False
            self.said = Said(clock, caused, later)
            self.owner = Said(clock, owner_caused, owner_later)
            self.owner.subscription, self.owner.holder = 2, 6
            self.baseline, self.meanwhile, self.owner_refused = baseline, list(meanwhile), owner_refused
            self.presses, self.subscribed_before_keys, self.owner_before_keys = 0, None, None
            self.watched, self.owner_watched, self.asked_shapes = None, None, []

        def observe(self, producer, shapes, **options):
            if producer == "zengine.realization":
                if self.owner_refused:
                    from loom_session.tool import Refused
                    raise Refused("guest 'td-maker' may not observe RealizationAsked v1 from "
                                  "zengine.realization: its row's observe list does not name it")
                self.owner_watched = (producer, list(shapes), options)
                self.owner_before_keys = not self.keys()
                return self.owner
            self.watched = (producer, list(shapes), options)
            self.subscribed_before_keys = not self.keys()
            return self.said

        def ask(self, office, shape, fields, **options):
            self.asked_shapes.append(shape)
            if shape == "BuildStatusRequested":
                for spec in self.meanwhile:  # published before the answer: already arrived
                    self.said.ready.append(self.said.word(spec, 0))
                self.meanwhile = []
                if self.baseline is None:
                    from loom_session.tool import Refused
                    raise Refused("gate refused: BuildStatusRequested v1 is not granted")
                return SimpleNamespace(shape="BuildStatus", fields=dict(self.baseline[1]),
                                       correlation=900)
            said = Scripted.ask(self, office, shape, fields, **options)
            if shape == "InjectInput":
                self.presses += 1
                answer = PressAnswer(said)
                answer.correlation = 700 + self.presses
                if self.cause_fired and not getattr(self, "caused_sent", False):
                    self.caused_sent = True
                    self.said.press(answer.correlation)
                    self.owner.press(answer.correlation)
                return answer
            return said

        def typed(self, m):
            if m["kind"] == "KeyPressed" and (m["scancode"], m["modifiers"]) == self.trigger:
                self.fired = True
            if m["kind"] == "KeyPressed" and (m["scancode"], m["modifiers"]) == self.cause_key:
                self.cause_fired = True

        def view(self, provider, pane):
            if not self.fired:
                return self.frames[0]
            self.reads += 1
            return self.frames[min(self.reads, len(self.frames) - 1)]

    class BuilderChecks(unittest.TestCase):
        def run_builder(self, act, caused=(), later=(), frames=(BEFORE,), **inputs):
            clock = Clock()
            builder.time = clock
            ctx = BuilderScript(act, clock, caused, later, frames, **inputs)
            try:
                said = builder.run(ctx)
                error = None
            except CheckFailed as err:
                said, error = None, str(err)
            record = json.loads(ctx.produced["builder.json"]) if "builder.json" in ctx.produced else None
            return SimpleNamespace(said=said, error=error, record=record, ctx=ctx, clock=clock)

        def test_the_subscription_is_made_before_any_key_and_names_the_builders_two_words(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 2)])
            self.assertIsNone(r.error)
            self.assertTrue(r.ctx.subscribed_before_keys)
            producer, shapes, options = r.ctx.watched
            self.assertEqual(producer, "zengine.builder")
            self.assertEqual(shapes, [("BuildAsked", 1), ("BuildStatus", 4)])
            self.assertEqual(options.get("latest"), ["BuildStatus"])

        def test_frontier_follows_its_ask_to_the_build_and_then_its_realization(self):
            r = self.run_builder("frontier", caused=[asked(8, realize=True),
                                                     status(8, 0, 1, 1, True),
                                                     status(8, 8, 6, 1, True)],
                                 later=[status(8, 8, 6, 1, True), status(8, 8, 2, 2, True),
                                        status(8, 8, 2, 3, True, realized="reloaded in place")],
                                 realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertIn("ask 8", r.said)
            self.assertIn("op #8 build succeeded", r.said)
            self.assertIn("realization realized", r.said)
            self.assertEqual(r.record["ask"]["number"], 8)
            self.assertIn("caused by this press", r.record["ask"]["attributed_by"])
            self.assertEqual(r.record["operation"]["op"], 8)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            self.assertIn((LETTERS["f"], 0), r.ctx.keys())

        def test_an_ending_already_in_what_the_press_set_in_motion_is_accepted(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 0, 1), status(8, 8, 6),
                                                  status(8, 8, 2, detail="built")])
            self.assertIsNone(r.error, r.error)
            self.assertIn("op #8 build succeeded", r.said)
            self.assertIn("realization not asked", r.said)

        def test_a_pending_realization_stays_unresolved_naming_its_ask_and_operation(self):
            r = self.run_builder("frontier", caused=[asked(8, realize=True), status(8, 8, 6, 1, True)],
                                 later=[status(8, 8, 2, 2, True)])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("realization is pending", r.error)
            self.assertIn("look op=8", r.error)
            self.assertEqual(r.record["unresolved"]["op"], 8)
            self.assertEqual(r.record["unresolved"]["ask"], 8)

        def test_expect_any_does_not_turn_a_running_build_into_an_ending(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 6)],
                                 later=[status(8, 8, 6)], expect="any")
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("build has not ended", r.error)

        def test_a_press_that_asked_nothing_says_so_in_the_panes_own_words(self):
            r = self.run_builder("build", frames=[BEFORE, rows("no recipe is chosen: nothing was "
                                                               "asked for -- pick one")])
            self.assertIn("asked nothing", r.error)
            self.assertIn("pick one", r.error)
            self.assertIsNone(r.record["ask"]["number"])

        def test_a_refused_ask_is_not_taken_and_keeps_the_builders_words(self):
            refused = [asked(0, taken=False, refusal="a build is already running: operation #7"),
                       status(7, 7, 6, detail="a build is already running: operation #7")]
            r = self.run_builder("build", caused=refused)
            self.assertIn("did not succeed", r.error)
            self.assertIn("already running", r.error)
            self.assertEqual(r.record["build"]["outcome"], "not taken")
            r = self.run_builder("build", caused=refused, expect="failed")
            self.assertIsNone(r.error, r.error)

        def test_another_presss_ask_is_never_this_ones(self):
            r = self.run_builder("build", caused=[other(asked(8)), asked(9), status(9, 9, 6)],
                                 later=[status(9, 9, 2)])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["ask"]["number"], 9)
            self.assertIn("op #9 build succeeded", r.said)

        def test_a_later_ask_before_this_ones_ending_is_superseded(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 0, 1)],
                                 later=[status(9, 9, 6), status(8, 8, 2)])
            self.assertIn("SUPERSEDED", r.error)
            self.assertIn("ask 9 before ask 8", r.error)
            self.assertEqual(r.record["superseded"]["ask"], 8)

        def test_an_unknown_recipe_refusal_during_the_build_is_set_aside(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 6)],
                                 later=[other(asked(0, taken=False, refusal="no recipe called x")),
                                        status(8, 8, 5, detail="no recipe called x"),
                                        status(8, 8, 2, detail="built")])
            self.assertIsNone(r.error, r.error)
            self.assertIn("op #8 build succeeded", r.said)
            self.assertEqual(len(r.record["set_aside"]), 1)

        def test_a_failed_build_is_judged_by_expect_and_its_realization_reported_apart(self):
            failed = [asked(8, realize=True), status(8, 8, 6, 1, True),
                      status(8, 8, 3, 4, True, realized="the build failed, so nothing was offered")]
            r = self.run_builder("frontier", caused=failed)
            self.assertIn("did not succeed", r.error)
            self.assertIn("output.txt", r.ctx.produced)
            r = self.run_builder("frontier", caused=failed, expect="failed", realize="refused")
            self.assertIsNone(r.error, r.error)
            self.assertIn("realization REFUSED", r.said)

        def test_realize_judges_the_realization_alone(self):
            took = [asked(8, realize=True), status(8, 8, 2, 4, True, realized="the plan refused it")]
            r = self.run_builder("frontier", caused=took, realize="realized")
            self.assertIn("the realization was REFUSED, not realized", r.error)
            r = self.run_builder("frontier", caused=took, realize="refused")
            self.assertIsNone(r.error, r.error)

        def test_a_plain_build_asked_nothing_of_realization(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 2)], realize="refused")
            self.assertIn("the realization was not asked, not refused", r.error)

        def test_a_gap_then_a_later_status_about_this_ask_still_decides(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 6)],
                                 later=[GAP, status(8, 8, 2)])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(len(r.record["gaps"]), 1)

        def test_an_observation_that_ends_is_unresolved_with_what_is_known(self):
            r = self.run_builder("build", caused=[asked(8), status(8, 8, 6)], later=[LOST])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("observation ended (lost)", r.error)
            self.assertEqual(r.record["unresolved"]["op"], 8)

        def test_two_asks_caused_by_one_press_cannot_be_told_apart(self):
            r = self.run_builder("build", caused=[asked(8), asked(9)])
            self.assertIn("made 2 asks", r.error)

        def test_arm_confirms_the_switch_and_never_toggles_it_blind(self):
            r = self.run_builder("arm", frames=[BEFORE, rows(armed=True)])
            self.assertIsNone(r.error, r.error)
            self.assertIn((LETTERS["b"], 1), r.ctx.keys())
            r = self.run_builder("arm", frames=[BEFORE, rows(armed=False)])  # the switch turned off
            self.assertIn("switch did not answer", r.error)
            r = self.run_builder("arm", frames=[rows(armed=True)])
            self.assertIsNone(r.error, r.error)
            self.assertNotIn((LETTERS["b"], 1), r.ctx.keys())  # already on: nothing pressed
            waiting = rows(realize="-- (load-after-build loads tower-defense now)")
            r = self.run_builder("arm", frames=[waiting])
            self.assertIn("use act=load-built", r.error)

        def test_load_built_needs_a_built_product_waiting(self):
            r = self.run_builder("load-built", frames=[BEFORE])
            self.assertIn("nothing built is waiting", r.error)
            self.assertEqual(r.ctx.keys()[-1:], [])  # nothing pressed

        # ---- promote and revert: the realization owner's numbered asks ------------------------
        def test_promote_is_answered_by_the_owners_word_naming_this_presss_ask(self):
            r = self.run_builder("promote", owner_caused=[owner_asked(4, "promote"), promoted(4)])
            self.assertIsNone(r.error, r.error)
            self.assertTrue(r.ctx.owner_before_keys)
            producer, shapes, _ = r.ctx.owner_watched
            self.assertEqual(producer, "zengine.realization")
            self.assertEqual(shapes, [("RealizationAsked", 1), ("ArtifactRealized", 3),
                                      ("ArtifactPromoted", 2)])
            self.assertEqual(r.record["ask"]["number"], 4)
            self.assertIn("caused by this press", r.record["ask"]["attributed_by"])
            self.assertTrue(r.record["realization"]["promote"])
            self.assertEqual(r.record["realization"]["answered_by"], "ArtifactPromoted naming ask 4")
            self.assertEqual(r.ctx.keys().count((LETTERS["p"], 1)), 1)

        def test_promote_refused_by_its_owner_is_that_owners_word(self):
            r = self.run_builder("promote", owner_caused=[
                owner_asked(4, "promote"), promoted(4, False, "'tower-defense' could not be "
                                                              "promoted: disk full")])
            self.assertIn("the owner did not promote", r.error)
            self.assertIn("disk full", r.error)
            self.assertFalse(r.record["realization"]["promote"])

        def test_another_operations_promotion_never_completes_this_press(self):
            # The review's reproduction: the pane named op #7, and op #9's promotion -- set in
            # motion by correlation 999, not this press -- arrived. This press caused no ask.
            r = self.run_builder("promote", caused=[other(status(9, 9, 2, 3, True, realized="promoted: "
                                                                "another operation", default=True))],
                                 owner_caused=[other(owner_asked(9, "promote")), other(promoted(9))])
            self.assertIsNotNone(r.error)
            self.assertIn("asked nothing by this press", r.error)
            self.assertIsNone(r.record["realization"])
            # ...and an answer naming another ask, after this press's own ask, is set aside.
            r = self.run_builder("promote", owner_caused=[owner_asked(4, "promote")],
                                 owner_later=[promoted(3)])
            self.assertIn("UNRESOLVED", r.error)
            self.assertEqual(r.record["set_aside"][0]["ask"], 3)

        def test_an_old_matching_status_for_the_same_operation_is_not_the_answer(self):
            old = status(7, 7, 2, 3, True, realized="promoted: the next launch runs the image weave "
                                                    "#31", default=True)
            r = self.run_builder("promote", caused=[old], owner_caused=[owner_asked(4, "promote")],
                                 owner_later=[promoted(2, detail="promoted: an earlier ask")])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("ask 4", r.error)
            r = self.run_builder("promote", caused=[old], owner_caused=[owner_asked(4, "promote")],
                                 owner_later=[promoted(2), promoted(4)])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["ask"], 4)

        def test_a_revert_is_answered_when_its_reload_settles_by_its_number(self):
            # Taken now; answered later in nobody's dispatch (cause 0); an unrelated refusal about
            # the same artifact -- an offer refused because the reload is open -- comes between.
            r = self.run_builder("revert", owner_caused=[owner_asked(6, "revert")],
                                 owner_later=[other(owner_asked(0, "offer", False, "a reload is open")),
                                              realized(0, False, "a reload of 'tower-defense' is "
                                                                 "already open"),
                                              realized(6)])
            self.assertIsNone(r.error, r.error)
            self.assertTrue(r.record["realization"]["revert"])
            self.assertEqual(r.record["realization"]["cause"], 0)
            self.assertEqual(r.record["realization"]["answered_by"], "ArtifactRealized naming ask 6")
            self.assertEqual(len(r.record["set_aside"]), 1)
            self.assertEqual(r.ctx.keys().count((LETTERS["r"], 1)), 1)

        def test_a_revert_refused_now_or_failed_later_is_the_owners_word(self):
            r = self.run_builder("revert", owner_caused=[
                owner_asked(0, "revert", False, "artifact 'tower-defense' has no previous image to "
                                                "revert to"),
                realized(0, False, "artifact 'tower-defense' has no previous image to revert to")])
            self.assertIn("did not take the revert", r.error)
            self.assertIn("no previous image", r.error)
            r = self.run_builder("revert", owner_caused=[owner_asked(6, "revert")],
                                 owner_later=[realized(6, False, "the kernel refused the image")])
            self.assertIn("the owner did not revert", r.error)
            self.assertEqual(r.record["realization"]["outcome"], "REFUSED")

        def test_a_caused_word_that_is_not_the_answer_leaves_the_revert_unresolved(self):
            republished = status(7, 7, 2, 3, True, realized="reverted: an earlier revert")
            r = self.run_builder("revert", caused=[republished],
                                 owner_caused=[owner_asked(6, "revert")])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("revert ask 6", r.error)
            self.assertIn("Workshop relay R", r.error)
            self.assertEqual(r.record["unresolved"]["ask"], 6)
            self.assertEqual(r.ctx.keys().count((LETTERS["r"], 1)), 1)  # never pressed again

        def test_a_replaced_owner_counting_afresh_does_not_answer_this_ask(self):
            r = self.run_builder("revert", owner_caused=[owner_asked(6, "revert")],
                                 owner_later=[by(realized(6), 42)])
            self.assertIn("UNRESOLVED", r.error)
            self.assertEqual(len(r.record["set_aside"]), 1)

        def test_promote_needs_the_owners_words_before_it_presses(self):
            r = self.run_builder("promote", owner_refused=True)
            self.assertIn("cannot be followed here", r.error)
            self.assertEqual(r.ctx.keys(), [])

        def test_load_it_is_confirmed_by_the_plan_rows_sentence_and_follows_a_load_now(self):
            role_line = BEFORE + ["role for tower-defense> "]
            said_pending = rows("loaded `tower-defense` as td.game -- pending -- the project is "
                                "waiting on it")
            r = self.run_builder("load-it", frames=[BEFORE, role_line, said_pending], role="td.game")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["outcome"], "pending")
            loading = rows("loading `tower-defense` now -- Workshop stays live")
            r = self.run_builder("load-it", frames=[BEFORE, role_line, loading], role="td.game",
                                 caused=[asked(8, realize=True), status(8, 8, 6, 1, True)],
                                 later=[status(8, 8, 2, 3, True, realized="loaded")])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["ask"]["number"], 8)
            self.assertEqual(r.record["realization"]["outcome"], "realized")

        # ---- look: coming back to an operation ----------------------------------------------
        # No pane is described in any of these (frames=[[]]): a look reads none, and takes no input
        # session. `baseline` is the Builder's own answer; `later` what follows it.
        NO_PANE = [[]]
        TOUCHES = ("InputSessionRequested", "InjectInput", "PaneViewRequested", "PanePointRequested")

        def look(self, baseline, **options):
            options.setdefault("frames", self.NO_PANE)
            r = self.run_builder("look", baseline=baseline, op=options.pop("op", 8), **options)
            self.assertEqual([s for s in r.ctx.asked_shapes if s in self.TOUCHES], [])
            self.assertEqual(r.ctx.keys(), [])
            return r

        def test_look_needs_no_pane_and_no_input_and_follows_the_build_to_its_end(self):
            r = self.look(status(8, 8, 6), later=[status(8, 8, 6), status(8, 8, 2)])
            self.assertIsNone(r.error, r.error)
            self.assertIn("op #8 build succeeded", r.said)
            self.assertIn("realization not asked", r.said)
            self.assertEqual(r.ctx.asked_shapes, ["BuildStatusRequested"])
            self.assertTrue(r.ctx.subscribed_before_keys)

        def test_look_keeps_an_offered_realization_pending(self):
            # The review's first reproduction: op #8 built, its realization OFFERED.
            r = self.look(status(8, 8, 2, 2, True), later=[status(8, 8, 2, 2, True)])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("realization is pending", r.error)
            self.assertIn("look op=8 relay=R", r.error)
            self.assertEqual(r.record["unresolved"]["numbered_by"]["relay"], "R")
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            self.assertIsNone(r.record["realization"])
            r = self.look(status(8, 8, 2, 2, True), later=[status(8, 8, 2, 2, True)], expect="any")
            self.assertIn("UNRESOLVED", r.error)

        def test_look_accepts_a_realization_completed_before_it_came(self):
            # The review's second reproduction: op #8 already REALIZED.
            r = self.look(status(8, 8, 2, 3, True, realized="reloaded in place"), realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            self.assertIn("answer to this look", r.record["realization"]["from"])

        def test_look_follows_a_realization_that_completes_later(self):
            r = self.look(status(8, 8, 2, 2, True), later=[status(8, 8, 2, 3, True, realized="loaded")],
                          realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            refused = status(8, 8, 2, 4, True, realized="the plan refused it")
            r = self.look(status(8, 8, 2, 2, True), later=[refused], realize="refused")
            self.assertIsNone(r.error, r.error)
            self.assertIn("realization REFUSED", r.said)
            r = self.look(status(8, 8, 2, 2, True), later=[refused], realize="realized")
            self.assertIn("the realization was REFUSED, not realized", r.error)

        def test_look_tells_a_plain_build_from_a_pending_one(self):
            r = self.look(status(8, 8, 2))
            self.assertIsNone(r.error, r.error)
            self.assertFalse(r.record["realization"]["asked"])
            r = self.look(status(8, 8, 2), realize="realized")
            self.assertIn("the realization was not asked, not realized", r.error)

        def test_look_joins_an_ending_that_arrived_with_its_answer(self):
            # Op #8 finished while the look was attaching: the answer still says running, and the
            # word that ended it is already here.
            r = self.look(status(8, 8, 6, 1, True),
                          meanwhile=[status(8, 8, 2, 2, True), status(8, 8, 2, 3, True)])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            self.assertEqual(r.record["realization"]["outcome"], "realized")

        def test_look_never_adopts_a_later_operation(self):
            r = self.look(status(8, 8, 2, 2, True), later=[status(9, 0, 1)])
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("moved on to ask 9", r.error)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            r = self.look(status(9, 9, 6))
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("moved on to op #9", r.error)

        def test_look_refuses_a_number_this_builder_never_gave(self):
            r = self.look(status(5, 5, 2))
            self.assertIn("numbered no operation #8", r.error)
            r = self.look(status(0, 0, 0))
            self.assertIn("numbered no operation #8 -- it has taken no ask", r.error)
            r = self.look(status(8, 8, 2), relay="another-workshop")
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("another-workshop", r.error)
            self.assertNotIn("BuildStatusRequested", r.ctx.asked_shapes)
            # Said only after the join: a word of op #8 that came with the answer is op #8's.
            r = self.look(status(7, 7, 2), meanwhile=[status(8, 8, 2)])
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")

        def test_look_joins_what_arrived_with_its_answer_before_judging_an_unnumbered_ask(self):
            # Op #8 realized while the look asked, and the answer speaks of the NEXT ask, which the
            # Builder took and has not numbered (op 0): the completion that arrived decides.
            done = status(8, 8, 2, 3, True, realized="reloaded in place")
            r = self.look(status(9, 0, 1), meanwhile=[done], realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["build"]["op"], 8)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            self.assertEqual(r.record["realization"]["from"], "observed #1")
            self.assertEqual(r.record["before"]["op"], 0)
            # ...and the same when the next ask is already numbered.
            r = self.look(status(9, 9, 6), meanwhile=[done], realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["outcome"], "realized")

        def test_look_takes_an_unnumbered_ask_for_no_proof_about_op_n(self):
            # No word of op #8, and the answer is an ask its runner has not answered: nothing says op
            # #8 was never numbered, nor where it stands. Pending, naming op #8 and its relay.
            r = self.look(status(9, 0, 1))
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("ask 9 is not numbered yet", r.error)
            self.assertIn("look op=8 relay=R", r.error)
            self.assertNotIn("numbered no operation", r.error)
            self.assertIsNone(r.record["build"])
            self.assertEqual(r.record["unresolved"]["op"], 8)
            r = self.look(status(9, 0, 1), expect="any")
            self.assertIn("UNRESOLVED", r.error)
            # The runner's answer places that ask: numbered past op #8, or given no number at all.
            r = self.look(status(9, 0, 1), later=[status(9, 9, 6)])
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("moved on to op #9", r.error)
            self.assertNotIn("numbered no operation", r.error)
            r = self.look(status(9, 0, 1), later=[status(9, 0, 4)])
            self.assertIn("moved on to ask 9", r.error)
            # An answer its runner already gave decides at once.
            r = self.look(status(9, 0, 4))
            self.assertIn("moved on to ask 9", r.error)
            self.assertNotIn("numbered no operation", r.error)
            self.assertLess(r.clock.now - 1000.0, 1.0)

        def test_look_keeps_a_realization_pending_past_an_unnumbered_ask(self):
            # Op #8 built and its realization OFFERED; the answer is the next ask, not yet numbered.
            offered = status(8, 8, 2, 2, True)
            r = self.look(status(9, 0, 1), meanwhile=[offered], realize="realized")
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("op #8's build ended succeeded and its realization is pending", r.error)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")
            self.assertIsNone(r.record["realization"])
            r = self.look(status(9, 0, 1), meanwhile=[offered], expect="any")
            self.assertIn("UNRESOLVED", r.error)
            # A realization said before that answer and told after it still completes op #8...
            r = self.look(status(9, 0, 1), meanwhile=[offered],
                          later=[status(8, 8, 2, 3, True, realized="loaded")], realize="realized")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["realization"]["outcome"], "realized")
            # ...and the next ask's own word, once it is told, says none is coming.
            r = self.look(status(9, 0, 1), meanwhile=[offered], later=[status(9, 0, 1)])
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("moved on to ask 9", r.error)
            self.assertIn("its build ended succeeded and its realization ended", r.error)
            self.assertIsNone(r.record["realization"])

        def test_look_reads_a_failed_or_refused_op_n_from_what_came_with_its_answer(self):
            failed = status(8, 8, 3, 4, True, realized="the build failed, so nothing was offered")
            r = self.look(status(9, 0, 1), meanwhile=[failed], expect="failed", realize="refused")
            self.assertIsNone(r.error, r.error)
            self.assertEqual(r.record["build"]["outcome"], "FAILED")
            self.assertEqual(r.record["realization"]["outcome"], "REFUSED")
            self.assertIn("not read by a look", r.record["output"])
            r = self.look(status(9, 0, 1), meanwhile=[failed])
            self.assertIn("op #8's build did not succeed", r.error)
            refused = status(8, 8, 2, 4, True, realized="the plan refused it")
            r = self.look(status(9, 0, 1), meanwhile=[refused], realize="realized")
            self.assertIn("the realization was REFUSED, not realized", r.error)
            self.assertEqual(r.record["build"]["outcome"], "succeeded")

        def test_look_at_another_relays_number_adopts_nothing(self):
            r = self.look(status(9, 0, 1), meanwhile=[status(8, 8, 2, 3, True)],
                          relay="another-workshop", realize="realized")
            self.assertIn("CANNOT ESTABLISH op #8", r.error)
            self.assertIn("another-workshop", r.error)
            self.assertNotIn("BuildStatusRequested", r.ctx.asked_shapes)
            self.assertIsNone(r.record["build"])
            self.assertIsNone(r.record["realization"])
            self.assertEqual(r.record["cannot_establish"]["op"], 8)
            self.assertEqual(r.record["cannot_establish"]["numbered_by"]["relay"], "R")

        def test_look_that_loses_its_observation_is_unresolved(self):
            r = self.look(status(8, 8, 2, 2, True), later=[LOST])
            self.assertIn("UNRESOLVED", r.error)
            self.assertIn("observation ended (lost)", r.error)
            # ...and when the ending arrived with the answer, behind a gap, both are said.
            r = self.look(status(8, 8, 2, 2, True), meanwhile=[GAP, LOST])
            self.assertIn("observation ended (lost)", r.error)
            self.assertEqual(len(r.record["gaps"]), 1)

        def test_look_without_the_builders_answer_says_so(self):
            r = self.look(None)
            self.assertIn("could not ask the Builder where it stands", r.error)

        def test_look_at_a_failed_build_reads_no_output(self):
            r = self.look(status(8, 8, 3), expect="failed")
            self.assertIsNone(r.error, r.error)
            self.assertIn("not read by a look", r.record["output"])
            self.assertNotIn("output.txt", r.ctx.produced)

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
