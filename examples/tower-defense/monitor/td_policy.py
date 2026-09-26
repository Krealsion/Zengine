# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""tower-defense/monitor -- play the tower-defense example under a policy, and say whether play
went as it should: FINISHED, NEEDS ATTENTION or INCONCLUSIVE, with the evidence. The monitor
itself -- subscribing, waiting, deciding, keeping evidence -- is the workshop package's
(monitor.py); this file is the policy, and a maker edits it or its inputs, never Workshop or Loom.

THE POLICY. The CHECKPOINT is one road cell ("x,y"). Every enemy that steps onto it is one
CROSSING -- the game's own TdOccurred `step` at that cell, numbered with no gaps in each game (td.cpp).
More than THRESHOLD crossings in one game NEEDS ATTENTION, and so does a lost game; all five waves
held within the threshold is FINISHED. On attention, `on_attention=pause` pauses the game with `p`
through this run's own input session -- the guest's input power, not its observation -- and counts
it done only when the game's own TdSeen, caused by that press, says it is paused. The game is left
paused; `p` in its pane resumes it.

WHAT COUNTS. Only this run's game: the one its own `r` began (the `game` occurrence that press
caused names its number) -- or, with `plan=watch`, the one being played when it joined; crossings
said before it joined are not known, so a finish under the threshold is then INCONCLUSIVE. A count
needs every crossing: a loss the relay or the client reports, or a hole in the game's own numbering
(a reload in place drops what the old image had not said), is INCONCLUSIVE unless the threshold
was already crossed. A picture of where the enemies stand never stands in for the steps between.

THE PLANS (plans.json beside this file): `win` is the story's own play session, which the rules
win with 2 lives -- 8 crossings of 21,7, all in wave 5; `thin` builds nothing before wave 2 and
loses in wave 3, its 9th crossing of 21,7 coming in wave 3 while lives remain; `watch` plays
nothing and follows whatever game is being played."""
import json
import os

from monitor import Attention, Inconclusive, Monitor

TD = ["td.game", "td"]
HEADER = TD + ["TOWER DEFENSE"]
RUNNING = 1
WAVES = 5
PLANS = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "plans.json"),
                       encoding="utf-8"))


def cell(x, y):
    """Where a map cell is painted in the pane: row 0 is the header, a cell two columns wide."""
    return y + 1, 1 + 2 * x


class TowerDefense(object):
    producer = "td.game"
    shapes = [("TdSeen", 1), ("TdOccurred", 1)]
    latest = ["TdSeen"]  # where the game stands: a newer one says all an older one did
    window = 1024
    pane = TD

    def __init__(self, inputs):
        self.plan_name = inputs.get("plan", "win")
        if self.plan_name != "watch" and (self.plan_name.startswith("_") or self.plan_name not in PLANS):
            raise ValueError("plan is watch or one of plans.json's: %s"
                             % ", ".join(k for k in PLANS if not k.startswith("_")))
        if inputs.get("on_attention", "pause") not in ("pause", "report"):
            raise ValueError("on_attention is pause or report")
        self.plan = PLANS.get(self.plan_name, {})
        x, _, y = str(inputs.get("checkpoint", "21,7")).partition(",")
        self.checkpoint = (int(x), int(y))
        self.threshold = int(inputs.get("threshold", 8))
        self.on_attention = inputs.get("on_attention", "pause")
        self.pictures = bool(inputs.get("pictures", False))
        self.wave_seconds = float(self.plan.get("wave_seconds", 120))
        build = inputs.get("build", "")
        self.build = ({"act": build, "recipe": inputs.get("recipe", "tower-defense"),
                       "realize": "realized"} if build else None)
        self.game = None          # this run's game: its number
        self.joined_after = 0     # watching: the game's own count of occurrences before joining
        self.last_n = None        # the last TdOccurred number of this game seen
        self.crossings = []       # this game's crossings of the checkpoint, in order
        self.other_games = 0      # occurrences of any other game: never counted
        self.raised = False

    # ---- what the game says -------------------------------------------------------------------

    def observe(self, m, o):
        if o.shape == "TdSeen":
            if self.game is None and self.plan_name == "watch":
                # WATCHING JOINS HERE: the game's own count says how much was said before.
                self.game, self.joined_after, self.last_n = o["game"], o["occurred"], o["occurred"]
            return
        if self.game is None:
            return  # said before this run's game began: not about it
        if o["game"] != self.game:
            self.other_games += 1
            return
        if self.last_n is not None and o["n"] != self.last_n + 1 and not self.raised:
            raise Inconclusive("game %d's own numbering went from %d to %d: %d occurrence(s) were "
                               "never said, and a crossing may be among them"
                               % (self.game, self.last_n, o["n"], o["n"] - self.last_n - 1))
        self.last_n = o["n"]
        if o["what"] == "step" and (o["x"], o["y"]) == self.checkpoint:
            self.crossings.append(o)
            if len(self.crossings) > self.threshold and not self.raised:
                self.raised = True
                raise Attention("%d crossings of %d,%d in game %d, more than the threshold %d: the "
                                "last, enemy %d, in wave %d at tick %d"
                                % (len(self.crossings), self.checkpoint[0], self.checkpoint[1],
                                   self.game, self.threshold, o["enemy"], o["wave"], o["tick"]),
                                self.crossings)

    def gap(self, m, g):
        if not self.raised:
            raise Inconclusive("%d observation(s) of the game were lost (%s): the crossings among "
                               "them cannot be counted" % (g.lost, g.reason))

    # ---- playing ------------------------------------------------------------------------------

    def play(self, m):
        if self.plan_name == "watch":
            return self.watch(m)
        m.act([{"into": HEADER}])
        new = m.press("r")
        began = m.caused(new, lambda o: o.shape == "TdOccurred" and o["what"] == "game",
                         "the new game this press began")
        self.game, self.last_n = began["game"], began["n"]
        rounds = self.plan["rounds"]
        for n, r in enumerate(rounds, 1):
            for x, y in r["towers"]:
                # A press chooses the cell, a second press on it builds (td.cpp's PanePressed).
                presses = [m.press_at(*(TD + list(cell(x, y)))) for _ in range(2)]
                built = [o for o in m.caused_by(presses) if o.shape == "TdOccurred"
                         and o["what"] == "tower" and (o["x"], o["y"]) == (x, y)]
                if not built:
                    rows = m.rows(TD[0], TD[1], "tower-%d-%d-rows" % (x, y))
                    raise Attention("the plan's tower at %d,%d was not built: the game says %r"
                                    % (x, y, (rows or ["?"])[-2:]))
            m.act([{"into": HEADER}])
            wave = m.press("space")
            m.caused(wave, lambda o: o.shape == "TdOccurred" and o["what"] == "wave" and
                     o["wave"] == n, "wave %d begun by this press" % n)
            if self.pictures and n == WAVES:
                entered = [0]

                def tenth(o):
                    if o.shape == "TdOccurred" and o["game"] == self.game and o["what"] == "enter":
                        entered[0] += 1
                    return entered[0] >= 10
                m.wait_for(tenth, self.wave_seconds, "the tenth enemy of wave %d" % n)
                m.picture("last-wave")
            end = m.wait_for(lambda o: o.shape == "TdOccurred" and o["game"] == self.game and
                             o["what"] in ("held", "won", "lost"),
                             self.wave_seconds, "how wave %d ended" % n)
            if end["what"] == "lost":
                raise Attention("game %d was lost in wave %d" % (self.game, end["wave"]), [end])
            if end["what"] == "won":
                return self.won(m, end)
        raise Attention("the plan's %d rounds were played and game %d is not won" % (len(rounds), self.game))

    def watch(self, m):
        m.wait_for(lambda o: o.shape == "TdSeen", 30.0, "the game's next word (an idle game says none)")
        m.record["joined"] = {"game": self.game, "after_occurrence": self.joined_after}
        end = m.wait_for(lambda o: o.shape == "TdOccurred" and o["game"] == self.game and
                         o["what"] in ("won", "lost"), m.seconds, "how game %d ended" % self.game)
        if end["what"] == "lost":
            raise Attention("game %d was lost in wave %d" % (self.game, end["wave"]), [end])
        if self.joined_after > 0:
            raise Inconclusive("game %d was won with %d crossings of %d,%d seen, but this monitor "
                               "joined after its occurrence %d: the crossings before are not known"
                               % (self.game, len(self.crossings), self.checkpoint[0],
                                  self.checkpoint[1], self.joined_after))
        return self.won(m, end)

    def state_after(self, m, n, what):
        """WHERE THE GAME STANDS once occurrence `n` has been said. The game says what happened
        first and then where it stands (td.cpp's `tell`), and its TdSeen names the last occurrence
        it has said -- so the state that includes occurrence `n` is the first TdSeen of this game
        whose `occurred` reaches `n`. The newest one taken so far may be a tick older."""
        seen = m.latest("TdSeen")
        if seen is not None and seen["game"] == self.game and seen["occurred"] >= n:
            return seen
        return m.wait_for(lambda o: o.shape == "TdSeen" and o["game"] == self.game and
                          o["occurred"] >= n, 10.0, "the game's state after %s" % what)

    def won(self, m, end):
        """The game's word is the outcome; the pane must agree with it, or a broken picture would
        hide behind a typed finish."""
        seen = self.state_after(m, end["n"], "the win")
        rows = m.rows(TD[0], TD[1], "won")
        want = "wave %d/%d   gold %d   lives %d   YOU WIN" % (seen["wave"], WAVES, seen["gold"],
                                                               seen["lives"])
        if not rows or want not in rows[0]:
            raise Inconclusive("the game says it was won (%s) but its pane reads %r"
                               % (want, (rows or ["nothing"])[0]))
        if self.pictures:
            m.picture("won")
        return ("game %d won: all %d waves held with %d lives; %d crossing(s) of %d,%d, threshold %d"
                % (self.game, WAVES, seen["lives"], len(self.crossings), self.checkpoint[0],
                   self.checkpoint[1], self.threshold))

    # ---- acting on attention ------------------------------------------------------------------

    def attention(self, m, why):
        if self.on_attention != "pause":
            return {"done": False, "why": "on_attention=%s: reported, nothing done" % self.on_attention}
        seen = self.state_after(m, self.last_n or 0, "the attention") if self.game else None
        if seen is None or seen["phase"] != RUNNING or seen["paused"]:
            return {"done": False, "why": "nothing to pause: the game %s" % (
                "has said nothing" if seen is None else "is not running a wave" if
                seen["phase"] != RUNNING else "is already paused")}
        m.act([{"into": HEADER}])
        press = m.press("p")
        paused = m.caused(press, lambda o: o.shape == "TdSeen" and o["paused"] == 1,
                          "the game's own word that it paused")
        return {"done": True, "act": "pause", "press": press.correlation,
                "confirmed_by": {"seq": paused.seq, "delivery": paused.delivery,
                                 "published_in": paused.published_in, "tick": paused["tick"],
                                 "wave": paused["wave"], "lives": paused["lives"]}}

    def summary(self, m):
        return {"plan": self.plan_name, "checkpoint": list(self.checkpoint),
                "threshold": self.threshold, "on_attention": self.on_attention, "game": self.game,
                "joined_after": self.joined_after, "last_occurrence": self.last_n,
                "crossings": [{"seq": o.seq, "n": o["n"], "enemy": o["enemy"], "wave": o["wave"],
                               "tick": o["tick"], "cause": o.cause, "delivery": o.delivery,
                               "published_in": o.published_in} for o in self.crossings],
                "other_games_occurrences": self.other_games}


def run(ctx):
    try:
        policy = TowerDefense(ctx.inputs)
    except ValueError as err:  # a misspelled input costs nothing: nothing was asked of Workshop
        ctx.fail(str(err))
    return Monitor(ctx, policy).run()
