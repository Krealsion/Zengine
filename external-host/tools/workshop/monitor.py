# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A MONITOR: one run that prepares an application in Workshop, plays it, and watches what the
application's owner publishes under a POLICY, until the policy says FINISHED or NEEDS ATTENTION --
or the monitor cannot tell (INCONCLUSIVE) -- and keeps the evidence for whoever reads it next.

The policy is ordinary Python in its own tool package, beside the application it knows: its
loom-tool.json says ``"uses": ["workshop"]`` and its tool's ``run(ctx)`` is

    from monitor import Monitor
    def run(ctx):
        return Monitor(ctx, MyPolicy(ctx.inputs)).run()

A POLICY is an object with:

  producer, shapes, latest   the office watched, its shapes ``[("Name", version)]``, and which of
                             them are whole state (the relay may keep only the newest)
  build                      optional: a workshop/builder act's inputs, done before play
  play(m)                    acts through ``m`` and waits for what the owner says; returns its
                             words when play ends as planned (FINISHED)
  observe(m, o)              every observation, in order, before anything waits on it; raises
                             ``Attention`` when a threshold is crossed, ``Inconclusive`` when it
                             can no longer tell
  gap(m, g)                  optional: every loss the relay or this client reported. By default a
                             loss is INCONCLUSIVE -- a count that lost occurrences cannot be told
  attention(m, why)          optional: act on the application once attention is needed (a pause,
                             say), through ``m.press`` -- the run's own input authority, never the
                             observation's -- and confirm it by the owner's own word (``m.caused``)
  summary(m)                 optional: the policy's own state for monitor.json

WHAT ``m`` OFFERS. ``act(steps)`` runs workshop/act steps; ``press(chord)`` and ``press_at(...)``
are gestures whose answers carry this run's correlation, which an observation's ``cause`` names
when the gesture set it in motion; ``caused(answer, test)`` finds that observation;
``wait_for(test, seconds, what)`` waits for one the owner has not said yet; ``latest(shape)`` is
the newest of a state shape; ``picture(name)`` and ``rows(provider, pane, name)`` keep what
Workshop shows. Every wait is bounded by the policy's own seconds and by the monitor's deadline:
running out is the monitor's decision, INCONCLUSIVE, never evidence that the owner failed.

WHAT IS NOT THE MONITOR'S. The application's meaning (the policy's), the right to act on it (the
guest's input power: observing grants none), and whatever the owner goes on doing after the
observation ends -- a monitor that stops watching stops nothing.

EVIDENCE: monitor.json -- the outcome and why; the policy's summary; the subscription (its
identity, words said, lost, coalesced); the producer's identity (holder and incarnation); every
observation kept (bounded, the number let go said); the triggering observations with their cause
(this run's gesture) and the far host's history numbers (``delivery``, ``published_in``); the
newest of each state shape; each gesture and what confirmed it; the intervention, if any; and the
times. On attention, ``attention.png`` (or its cells) and ``attention-rows.json``."""
import json
import time

from loom_session.tool import LinkOutcome, NotAnswered, Refused

from act import act as act_step, painted, GESTURES, VERBS
from hand import Hand
from workshop_steps import chord_moments, moment, picture, png_of

FINISHED, ATTENTION, INCONCLUSIVE = "finished", "needs-attention", "inconclusive"
KEEP = 4000  # observations kept in monitor.json; the number let go is said


class Attention(Exception):
    """The policy's word: play is no longer as it should be. `observations` are the ones that
    showed it, kept with their identities as the evidence."""

    def __init__(self, words, observations=()):
        Exception.__init__(self, words)
        self.observations = list(observations)


class Inconclusive(Exception):
    """The monitor cannot tell: a loss it cannot count past, the observation ended, the producer
    replaced, a wait or the deadline ran out. Never a finish, never attention."""


class Monitor(object):
    def __init__(self, ctx, policy, link=None, seconds=None):
        self.ctx, self.policy = ctx, policy
        self.link = link or ctx.inputs.get("link", "workshop")
        self.seconds = float(seconds if seconds is not None else ctx.inputs.get("seconds", 900))
        self.began = time.monotonic()
        self.deadline = self.began + self.seconds
        # A pause after each gesture, so a person can watch; the owner's words decide as before.
        self.pace = max(0, int(ctx.inputs.get("pace_ms", 0) or 0)) / 1000.0
        self.hand = None
        self.sub = None
        self.kept, self.let_go = [], 0
        self.state = {}      # the newest observation of each `latest` shape
        self.holder = None   # (producer, incarnation) of the first observation
        self.gestures = []
        self.by_cause = {}   # this run's gesture correlation -> the observations it caused
        self.record = {"producer": policy.producer, "shapes": [list(s) for s in policy.shapes],
                       "latest": list(getattr(policy, "latest", ())), "seconds": self.seconds,
                       "phases": {}, "outcome": None}

    # ---- time and phases ------------------------------------------------------------------------

    def now(self):
        return round(time.monotonic() - self.began, 3)

    def phase(self, name):
        self.record["phases"].setdefault(name, self.now())
        self.ctx.step(name)

    # ---- the application: acting as a maker does --------------------------------------------

    def _hand(self):
        if self.hand is None or not self.hand.open:
            self.hand = Hand(self.ctx, self.link)
        return self.hand

    def release_keys(self):
        """Give Workshop's input session back now. A link that is gone cannot be asked --
        Workshop's door closes a lost guest's session itself -- so a failure here is written down,
        and the run's cleanup, which tries again, records what it could do."""
        if self.hand is not None and self.hand.open:
            try:
                self.hand.close()
            except Exception as err:
                self.record["input_session"] = "not given back from here: %s: %s" % (
                    type(err).__name__, str(err).splitlines()[0])

    def act(self, steps):
        """workshop/act steps through this run's input session; their records."""
        done = []
        for step in steps:
            verb = [v for v in VERBS if v in step]
            self.ctx.check(len(verb) == 1, "a step names exactly one verb: %r" % (step,))
            done.append(dict({"verb": verb[0], "args": step[verb[0]]},
                             **(act_step(self.ctx, self._hand(), verb[0], step) or {})))
            if self.pace and verb[0] in GESTURES:
                time.sleep(self.pace)
        return done

    def _gesture(self, what, answer):
        self.gestures.append({"gesture": what, "correlation": answer.correlation, "at": self.now()})
        if self.pace:
            time.sleep(self.pace)
        return answer

    def press(self, chord):
        """A chord, settled; the Input owner's answer (``answer.correlation`` is the cause an
        observation it set in motion carries)."""
        return self._gesture(chord, self._hand().inject(chord_moments(self.ctx, chord)))

    def press_at(self, provider, pane, row, column):
        """A press on one painted cell of a pane (its lattice, counted from 0)."""
        hand = self._hand()
        view = painted(hand, provider, pane)
        self.ctx.check(view is not None, "Workshop does not describe %s/%s now" % (provider, pane))
        at = hand.point(provider, pane, row, column, view["picture"])
        return self._gesture("press %s/%s %d,%d" % (provider, pane, row, column), hand.inject(
            [moment(self.ctx, "PointerButton", button=1, pressed=p, x=at["x"], y=at["y"],
                    space=at["space"]) for p in (True, False)]))

    def picture(self, name):
        """A capture kept as ``<name>.png`` (or ``<name>.cells.txt`` in a terminal medium)."""
        captured, data = picture(self.ctx, self.link, name)
        if captured["format"] == "image/bmp":
            self.ctx.produce(name + ".png", png_of(data))
        return {"frame": captured["frame"], "format": captured["format"]}

    def rows(self, provider, pane, name):
        view = painted(self._hand() if self.hand is not None and self.hand.open else _Asker(self),
                       provider, pane)
        rows = [r["text"] for r in view["rows"]] if view else None
        self.ctx.produce(name + ".json", json.dumps({"picture": view["picture"] if view else None,
                                                     "rows": rows}, indent=1).encode())
        return rows

    # ---- the owner's words --------------------------------------------------------------------

    def _take(self, item):
        """Everything that arrives passes here first, in order: kept, the producer's identity
        checked, the policy's `observe` or `gap` asked."""
        if item.kind == "ended":
            self.record["ended"] = item.record()
            raise Inconclusive("the observation ended (%s): %s" % (item.how, item.reason))
        if len(self.kept) < KEEP:
            self.kept.append(item.record())
        else:
            self.let_go += 1
        if item.kind == "gap":
            self.record.setdefault("gaps", []).append(item.record())
            handler = getattr(self.policy, "gap", None)
            if handler is None:
                raise Inconclusive("%d observation(s) were lost (%s): what they said cannot be "
                                   "counted" % (item.lost, item.reason))
            handler(self, item)
            return
        who = (item.producer, item.incarnation)
        if self.holder is None:
            self.holder = who
            self.record["holder"] = {"producer": item.producer, "incarnation": item.incarnation,
                                     "office": item.office, "link": item.link, "epoch": item.epoch,
                                     "session": item.session, "seq": item.seq}
        elif who != self.holder:
            raise Inconclusive("the office %s is held by another participant now (participant %d "
                               "incarnation %d, not %d/%d): what it says is not the game this "
                               "monitor watched" % (self.policy.producer, item.producer,
                                                    item.incarnation, self.holder[0], self.holder[1]))
        if item.shape in self.record["latest"]:
            self.state[item.shape] = item
        if item.cause:  # this run's own gestures only: a link strips every other asker's
            self.by_cause.setdefault(item.cause, []).append(item)
        self.policy.observe(self, item)

    def _next(self, bound):
        end = min(self.deadline, time.monotonic() + bound)
        return self.sub.next(max(0.0, end - time.monotonic()))

    def drain(self):
        """Take everything that has arrived, without waiting."""
        for item in self.sub.drain():
            self._take(item)

    def wait_for(self, test, seconds, what):
        """The next observation `test` accepts, within `seconds` (and the deadline). Every item
        on the way goes through the policy first, so a threshold crossed while this waits for
        something else still ends the wait."""
        end = min(self.deadline, time.monotonic() + seconds)
        while True:
            left = end - time.monotonic()
            item = self._next(left) if left > 0 else None
            if item is None:
                if time.monotonic() >= self.deadline:
                    raise Inconclusive("the monitor's own deadline (%gs) passed while it waited for "
                                       "%s" % (self.seconds, what))
                raise Inconclusive("%s was not said within %gs" % (what, seconds))
            self._take(item)
            if item.kind == "observed" and test(item):
                return item

    def caused_by(self, answers):
        """EVERYTHING THESE GESTURES CAUSED, in order. A settled gesture's caused observations
        arrive before its answer, so after the answers have returned this is complete."""
        self.drain()
        out = []
        for a in answers:
            out += self.by_cause.get(a.correlation, [])
        return out

    def caused(self, answer, test, what, seconds=5.0):
        """THE OBSERVATION A GESTURE CAUSED: the one `test` accepts whose ``cause`` is this
        gesture's correlation. A settled gesture's caused observations have arrived before its
        answer; `seconds` is slack for reading them, not a race."""
        corr = answer.correlation
        found = next((o for o in self.caused_by([answer]) if test(o)), None)
        if found is None:
            found = self.wait_for(lambda o: o.cause == corr and test(o), seconds,
                                  "%s (caused by gesture %d)" % (what, corr))
        for g in self.gestures:
            if g["correlation"] == corr:
                g.setdefault("confirmed_by", []).append({"seq": found.seq, "shape": found.shape,
                                                         "delivery": found.delivery})
        return found

    def latest(self, shape):
        return self.state.get(shape)

    # ---- the run ------------------------------------------------------------------------------

    def run(self):
        p, ctx = self.policy, self.ctx
        expect = ctx.inputs.get("expect", FINISHED)
        ctx.check(expect in (FINISHED, ATTENTION, "any"), "expect is finished, needs-attention or any")
        build = getattr(p, "build", None)
        if build:
            # FIRST THE BUILD, followed through the Builder's own words (workshop/builder): the
            # application's shapes exist on Workshop's bus only once it is loaded, and a relay
            # refuses a shape nobody declares. Nothing is asked of the application before the
            # subscription below.
            self.phase("build")
            import builder
            self.record["build"] = builder.perform(ctx, dict(build, link=self.link), name="build")
        self.phase("subscribe")
        # SUBSCRIBED BEFORE ANYTHING IS ASKED OF THE APPLICATION: every word from here on is kept.
        try:
            self.sub = ctx.observe(p.producer, p.shapes, via=self.link,
                                   latest=getattr(p, "latest", ()),
                                   window=int(getattr(p, "window", 1024)),
                                   label="%s (run %s)" % (type(p).__name__, ctx.name))
        except Refused as err:
            ctx.fail("Workshop's relay refused to let this run observe %s: %s" % (p.producer, err))
        self.record["subscription"] = {"subscription": self.sub.subscription, "relay": self.sub.relay,
                                       "holder": self.sub.holder, "incarnation": self.sub.incarnation,
                                       "window": self.sub.window}
        if self.sub.holder == 0:
            ctx.fail("nobody holds the office %s: the application is not running here%s"
                     % (p.producer, "" if build else ", and this monitor was not asked to build or "
                        "load it (its prerequisite)"))
        outcome, words, trigger = None, "", []
        try:
            self.phase("play")
            words = p.play(self) or "play ended as planned"
            outcome = FINISHED
        except Attention as why:
            outcome, words, trigger = ATTENTION, str(why), why.observations
            if trigger:  # from the deciding word's arrival here to the decision
                self.record["detected_after_s"] = round(time.monotonic() - trigger[-1].received, 4)
        except Inconclusive as why:
            outcome, words = INCONCLUSIVE, str(why)
        self.record.update(outcome=outcome, words=words, decided_at=self.now())
        self.record["trigger"] = [o.record() for o in trigger]
        try:
            if outcome == ATTENTION:
                self.phase("attention")
                self.record["intervention"] = self._intervene(words)
                self.record["evidence"] = self._evidence("attention")
            self.release_keys()
        finally:
            self._finish()  # THE OUTCOME IS WRITTEN whatever acting on it or giving back cost
        verdict = "%s: %s" % (outcome.upper(), words)
        if expect != "any" and outcome != expect:
            ctx.fail("%s (this monitor was to %s)" % (verdict, "finish" if expect == FINISHED
                                                      else "need attention"))
        return "%s (%.1fs)" % (verdict, time.monotonic() - self.began)

    def _intervene(self, why):
        handler = getattr(self.policy, "attention", None)
        if handler is None:
            return None
        try:
            return handler(self, why)
        except Inconclusive as err:
            return {"done": False, "why": str(err)}
        except Refused as err:  # no input authority: observing grants none
            return {"done": False, "refused": str(err)}
        except (LinkOutcome, NotAnswered) as err:  # the act's own outcome, not the monitor's
            return {"done": False, "why": "%s: %s" % (type(err).__name__, str(err).splitlines()[0])}

    def _evidence(self, name):
        kept = {}
        try:
            kept["picture"] = self.picture(name)
        except Exception as err:  # evidence that could not be taken is said, not fatal
            kept["picture"] = "not taken: %s: %s" % (type(err).__name__, err)
        rows = getattr(self.policy, "pane", None)
        if rows:
            try:
                kept["rows"] = self.rows(rows[0], rows[1], name + "-rows")
            except Exception as err:
                kept["rows"] = "not read: %s: %s" % (type(err).__name__, err)
        return kept

    def _finish(self):
        try:
            self.drain()  # what arrived meanwhile belongs in the record, not in a later task
        except (Attention, Inconclusive) as late:
            self.record["after_outcome"] = str(late)
        self.record["state"] = dict((s, o.record()) for s, o in self.state.items())
        self.record["gestures"] = self.gestures
        self.record["observations"] = self.kept
        self.record["observations_let_go"] = self.let_go
        summary = getattr(self.policy, "summary", None)
        self.record["policy"] = summary(self) if summary else None
        self.record["observation"] = self.sub.summary()
        self.record["elapsed_s"] = self.now()
        self.ctx.produce("monitor.json", json.dumps(self.record, indent=1).encode())


class _Asker(object):
    """Pane views asked without an input session (a watching monitor holds none)."""

    def __init__(self, m):
        self.m = m

    def view(self, provider, pane):
        return self.m.ctx.ask("zengine.workshop", "PaneViewRequested",
                              {"provider": provider, "pane": pane}, via=self.m.link)
