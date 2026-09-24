# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/builder -- drive Workshop's Builder pane as a maker would and wait for what it did:
choose a recipe, then build it, build what the project waits on, arm load-after-build, load what
was built, promote or revert the loaded image, or add the recipe's artifact to the load plan with
a role. loom-tool.json describes the acts.

THE PANE'S ROWS ARE THE EVIDENCE, read as one snapshot at a time (`observe`): the `exit` row's
`asks N ever` (how many build asks the Builder has taken), `last` (a build outcome and its
operation number), `realize` (a realization outcome and the operation it is about), `project`
(what the project waits on), the control strip's load-after-build switch, and the notice above
the rows (the pane's answer to a key). They are one BuildStatus the Builder publishes, painted:
a later observation route can fill the same snapshot from that typed status without changing a
verdict here.

WHAT COMPLETES EACH ACT. A build, the frontier and load-built are one OPERATION: the Builder must
TAKE the ask (`asks` goes up by exactly one), that operation's build must END (succeeded, FAILED,
NO ARTIFACT, did not start, unknown recipe), and, when the operation asked for realization, its
realization must END too (realized or REFUSED). `expect` judges the build and `realize` the
realization, two answers kept apart; running, asked and offered are never an ending. The other
acts start no build and have their own confirmation: arm, the switch reading on; promote and
revert, the owner's answer arriving on the realize row; load-it, the pane's own sentence about
the plan row -- and, when that sentence says the built product is being loaded now, the
operation that loads it, followed like a build.

UNRESOLVED IS NEITHER FAILED NOR PASSED. When the bounded wait ends first, the run fails as
UNRESOLVED, naming the operation and where it stands; the Builder carries on. `act=look op=N`
waits for that operation again and presses nothing. No key is ever pressed a second time.

A FAILED BUILD KEEPS ITS WORDS. When a build does not succeed the tool opens the Builder's output
reader (`l`), pages it to its last line and keeps every line it showed as output.txt, then closes
it. The reader shows what the build tool kept (bounded, in memory) cut to the pane's width."""
import json
import re
import time

from act import painted
from hand import Hand
from workshop_steps import chord_moments, moment

BUILDER = ("zengine.builder-pane", "builder")
LABELS = ("recipe", "project", "last", "exit", "ran", "realize", "said")
KEYS = {"build": "b", "frontier": "f", "arm": "shift+b", "load-built": "shift+b",
        "promote": "shift+p", "revert": "shift+r"}
ACTS = tuple(KEYS) + ("load-it", "look")
BUILD_ENDED = ("succeeded", "FAILED", "NO ARTIFACT", "did not start", "unknown recipe")
REALIZE_ENDED = ("realized", "REFUSED")
TAKE_SECONDS = 15.0  # a key the Builder answers takes its ask in one delivery; this is slack


def view_rows(hand):
    view = painted(hand, *BUILDER)
    return [r["text"] for r in view["rows"]] if view else []


def labelled(rows):
    return dict((r[:9].strip(), r[9:].strip()) for r in rows if r[:9].strip() in LABELS)


def op_of(text):
    """The operation a `last` or `realize` row names right after its first word (`op #8`)."""
    parts = (text or "").split(" -- ")
    found = re.match(r"op #(\d+)", parts[1]) if len(parts) > 1 else None
    return int(found.group(1)) if found else 0


def observe(hand):
    """One snapshot of the Builder's answers, from one PaneView."""
    rows = view_rows(hand)
    at = next((i for i, r in enumerate(rows) if r.startswith("BUILDER")), None)
    facts = labelled(rows)
    last, realize = facts.get("last", ""), facts.get("realize", "")
    asks = re.search(r"asks (\d+) ever", facts.get("exit", ""))
    strip = " ".join(rows)
    if realize.startswith("[x] load after build"):
        realization = "armed"
    elif realize.startswith("-- (load-after-build loads"):
        realization = "button"
    elif realize.startswith("--"):
        realization = "not asked"
    else:
        realization = realize.split(" -- ")[0].split(",")[0]
    armed = (True if "[turn load-after-build off]" in strip or realization == "armed"
             else False if "[turn load-after-build on]" in strip else None)
    return {"rows": rows, "notice": rows[at - 1] if at else (rows[0] if rows and at is None else ""),
            "header": at is not None, "last": last, "build": last.split(" -- ")[0],
            "op": op_of(last), "realize": realize, "realization": realization,
            "realize_op": op_of(realize), "asks": int(asks.group(1)) if asks else None,
            "armed": armed, "project": facts.get("project", ""), "recipe": facts.get("recipe", ""),
            "said": facts.get("said", "")}


def brief(o):
    return dict((k, o[k]) for k in ("notice", "last", "realize", "asks", "armed", "project"))


def press(ctx, hand, chord):
    hand.inject(chord_moments(ctx, chord))


def calm(ctx, hand):
    """The Builder's own rows: close its output reader, its recipe list or a role line first."""
    for _ in range(4):
        rows = view_rows(hand)
        ctx.check(rows, "Workshop does not describe the Builder pane; open it where nothing covers it")
        if rows[0].startswith("BUILDER") or (len(rows) > 1 and rows[1].startswith("BUILDER")):
            return rows
        at = hand.point(*BUILDER, 0, 0, painted(hand, *BUILDER)["picture"])
        hand.inject([moment(ctx, "PointerButton", button=1, pressed=p, x=at["x"], y=at["y"],
                            space=at["space"]) for p in (True, False)])
        press(ctx, hand, "escape")
    ctx.fail("the Builder did not return to its rows (it reads %r)" % view_rows(hand)[:2])


def keys_into(ctx, hand):
    view = painted(hand, *BUILDER)
    row = [r for r in view["rows"] if r["text"].startswith("BUILDER")][0]
    at = hand.point(*BUILDER, row["row"], 0, view["picture"])
    hand.inject([moment(ctx, "PointerButton", button=1, pressed=p, x=at["x"], y=at["y"],
                        space=at["space"]) for p in (True, False)])


def choose(ctx, hand, recipe):
    seen = []
    for _ in range(64):
        now = labelled(view_rows(hand)).get("recipe", "")
        if now.startswith(recipe + " ->"):
            return now
        ctx.check(now not in seen, "the catalog holds no recipe %r (it offered %s)" % (recipe, seen))
        seen.append(now)
        press(ctx, hand, "c")
        time.sleep(0.1)
    ctx.fail("could not choose %r" % recipe)


def read_output(ctx, hand):
    """Every line the reader shows, by its number: the header says `lines a-b of T`."""
    press(ctx, hand, "l")
    kept, header = {}, ""
    for _ in range(200):
        time.sleep(0.15)
        rows = view_rows(hand)
        heads = [i for i, r in enumerate(rows) if r.startswith("output #")]
        if not heads:
            continue
        header = rows[heads[0]]
        body = [r for r in rows[heads[0] + 1:] if not r.startswith("[menu]")]
        span = re.search(r"lines (\d+)-(\d+) of (\d+)", header)
        if not span:
            kept = dict(enumerate(body, 1))
            break
        first, end, total = (int(g) for g in span.groups())
        for i, text in enumerate(body[:end - first + 1]):
            kept[first + i] = text
        if end >= total:
            break
        hand.inject(chord_moments(ctx, "down", repeat=end - first + 1))
    press(ctx, hand, "escape")
    return header + "\n" + "\n".join(kept[n] for n in sorted(kept))


class Watch:
    """The bounded wait of one run: every distinct snapshot it saw, and what it came to."""

    def __init__(self, ctx, hand, record, seconds):
        self.ctx, self.hand, self.record = ctx, hand, record
        self.end = time.monotonic() + seconds
        self.seconds = seconds

    def look(self):
        now = observe(self.hand)
        seen = [now["last"], now["realize"], now["notice"]]
        if seen not in self.record["seen"]:
            self.record["seen"].append(seen)
        return now

    def until(self, test, stage, op=0, bound=None):
        """Read snapshots until `test(now)` answers something other than None; UNRESOLVED when the
        wait (or the smaller `bound`) runs out first."""
        end = min(self.end, time.monotonic() + bound) if bound else self.end
        while True:
            now = self.look()
            got = test(now)
            if got is not None:
                return now, got
            if time.monotonic() >= end:
                self.unresolved(now, stage, op)
            time.sleep(0.25)

    def unresolved(self, now, stage, op):
        self.record["after"] = brief(now)
        self.record["unresolved"] = {"stage": stage, "op": op, "last": now["last"],
                                     "realize": now["realize"], "notice": now["notice"],
                                     "said": now["said"]}
        self.ctx.produce("builder.json", json.dumps(self.record, indent=1).encode())
        again = (" `act=look op=%d` waits for it again and presses nothing." % op) if op else ""
        self.ctx.fail("UNRESOLVED: %s (last %r; realize %r; notice %r; said %r). The Builder "
                      "carries on with it; nothing was pressed twice.%s"
                      % (stage, now["last"], now["realize"], now["notice"], now["said"], again))


def follow(ctx, watch, before, record, op=None):
    """ONE OPERATION, from the Builder taking its ask to its build and realization ending. With `op`
    (look), no ask is waited for: that operation must still be the Builder's latest."""
    if op is None:
        def taken(now):
            # WHICH ASK IS THIS RUN'S. The `exit` row's count of asks taken says it exactly; a short
            # pane drops that row first when a notice or the project row needs the room, and then
            # the first operation NUMBER above the one standing before the press says it (the
            # Builder numbers each build it starts, one at a time). An older operation ending is
            # never taken for this one.
            if now["asks"] is not None and before["asks"] is not None:
                ctx.check(now["asks"] <= before["asks"] + 1, "the Builder took %d asks after this "
                          "press; this run cannot tell which one is its own"
                          % (now["asks"] - before["asks"]))
                if now["asks"] == before["asks"] + 1:
                    return "the asks counter"
            elif now["op"] > before["op"]:
                return "a new operation number"
            elif now["op"] == 0 and now["build"] in ("did not start", "unknown recipe") and \
                    now["last"] != before["last"]:
                return "a new outcome that names no operation"
            # THE PANE'S OWN REFUSALS end in these words (`builder-pane/pane.cpp`): it sent nothing.
            if now["notice"] != before["notice"] and ("nothing was asked for" in now["notice"] or
                                                      " -- pick one" in now["notice"]):
                ctx.fail("the Builder asked for nothing: %s" % now["notice"])
            return None
        now, how = watch.until(taken, "the Builder did not take this ask (asks %s ever before; last "
                               "%r before)" % (before["asks"], before["last"]), bound=TAKE_SECONDS)
        record["ask"] = {"asks_before": before["asks"], "asks_after": now["asks"], "attributed_by": how}
        if how == "a new operation number":
            op = now["op"]
    mine = op

    def ended(now):
        if mine is not None and now["op"] not in (0, mine):
            ctx.fail("op #%d is no longer the Builder's latest (it shows op #%d); its own words are in "
                     "the output reader's older builds" % (mine, now["op"]))
        return now["build"] if now["build"] in BUILD_ENDED else None
    now, outcome = watch.until(ended, "the build has not ended", op or 0)
    record["build"] = {"op": now["op"], "outcome": outcome}
    asked = now["realization"] in ("asked", "offered") + REALIZE_ENDED
    record["realization"] = {"asked": asked}
    if asked:
        def realized(snap):
            return snap["realization"] if (snap["realization"] in REALIZE_ENDED and
                                           snap["realize_op"] == now["op"]) else None
        now, outcome = watch.until(realized, "op #%d's build ended %s and its realization is pending"
                                   % (now["op"], record["build"]["outcome"]), now["op"])
        record["realization"].update(op=now["realize_op"], outcome=outcome, detail=now["realize"])
    return now


def load_it(ctx, hand, watch, before, record):
    """`o`, the role and Return; then the pane's own sentence about the plan row."""
    stem = before["recipe"].split("->")[-1].strip().split(" ")[0]
    ctx.check(stem, "no recipe is chosen (the recipe row reads %r)" % before["recipe"])
    press(ctx, hand, "o")

    def line_open(now):
        if any(r.startswith("role for %s>" % stem) for r in now["rows"]):
            return True
        return False if now["notice"] != before["notice"] and "type the role" not in now["notice"] \
            else None
    now, opened = watch.until(line_open, "`o` opened no role line for %s" % stem, bound=TAKE_SECONDS)
    ctx.check(opened, "the Builder did not open a role line for %s: %s" % (stem, now["notice"]))
    hand.inject([moment(ctx, "TextEntered", text=ctx.inputs["role"])])
    press(ctx, hand, "enter")
    loaded, loading = "loaded `%s` as " % stem, "loading `%s` now" % stem

    def answered(now):
        n = now["notice"]
        if n.startswith(loaded) or n.startswith(loading):
            return n
        if now["header"] and n and "type the role" not in n and n != before["notice"]:
            return n
        return None
    now, said = watch.until(answered, "the Builder did not answer the role for %s" % stem,
                            bound=TAKE_SECONDS)
    record["confirmation"] = said
    if said.startswith(loading):
        record["plan_row"] = "written; the built product is loaded now"
        return follow(ctx, watch, before, record)
    ctx.check(said.startswith(loaded), "the Builder did not add %s to the plan: %s" % (stem, said))
    detail = said[len(loaded):].split(" -- ", 1)[-1]
    state = next((w for w in ("resolved", "pending", "loading", "refused", "authored behind")
                  if detail.startswith(w)), "said")
    record["plan_row"] = "written"
    record["realization"] = {"asked": True, "outcome": {"authored behind": "pending"}.get(state, state),
                             "detail": detail}
    return now


def answer_of(ctx, watch, before, record, what):
    """Promote and revert start no build: the owner's answer lands on the realize row, about the
    same operation, in new words."""
    def moved(now):
        if "nothing to %s" % what in now["notice"]:
            ctx.fail("the Builder has nothing to %s: %s" % (what, now["notice"]))
        return True if now["realize"] != before["realize"] and now["realize_op"] == before["realize_op"] \
            else None
    now, _ = watch.until(moved, "no answer to %s arrived on the realize row" % what, before["realize_op"])
    word = "promoted:" if what == "promote" else "reverted"
    done = word in now["realize"] and (what == "revert" or "NOT DEFAULT" not in now["realize"])
    record["realization"] = {"asked": True, "op": now["realize_op"], "outcome": now["realization"],
                             "detail": now["realize"], what: done}
    return now, done


def run(ctx):
    what = ctx.inputs["act"]
    ctx.check(what in ACTS, "act must be one of %s" % ", ".join(ACTS))
    expect = ctx.inputs.get("expect", "succeeded")
    ctx.check(expect in ("succeeded", "failed", "any"), "expect is succeeded, failed or any")
    realize = ctx.inputs.get("realize", "any")
    ctx.check(realize in ("realized", "refused", "any"), "realize is realized, refused or any")
    ctx.check(what != "load-it" or ctx.inputs.get("role"), "load-it needs the role the artifact will hold")
    op = int(ctx.inputs.get("op", 0) or 0)
    seconds = float(ctx.inputs.get("seconds", 300))
    hand = Hand(ctx, ctx.inputs["link"])
    started = time.monotonic()
    calm(ctx, hand)
    keys_into(ctx, hand)
    if ctx.inputs.get("recipe") and what != "look":
        choose(ctx, hand, ctx.inputs["recipe"])
    before = observe(hand)
    record = {"act": what, "before": brief(before), "seen": [], "build": None, "realization": None}
    watch = Watch(ctx, hand, record, seconds)
    now, passed, said = before, True, ""
    if what == "look":
        if op:
            now = follow(ctx, watch, before, record, op=op)
    elif what == "arm":
        if before["armed"]:
            record["confirmation"] = "already on; nothing was pressed"
        else:
            ctx.check(before["realization"] != "button", "a build is waiting to be loaded (realize "
                      "reads %r): Shift+b would load it now, not arm the next one; use act=load-built. "
                      "Nothing was pressed" % before["realize"])
            press(ctx, hand, "shift+b")

            def switched(snap):
                # The control strip names the switch's state when it fits; otherwise the pane's
                # notice does, once it has changed from what it said before the press.
                if before["armed"] is not None and snap["armed"] is not None:
                    return "on" if snap["armed"] else None
                if snap["notice"] == before["notice"]:
                    return None
                return "on" if snap["notice"].startswith("load after build: on") else (
                    "off" if snap["notice"].startswith("load after build: off") else None)
            now, state = watch.until(switched, "the load-after-build switch did not answer",
                                     bound=TAKE_SECONDS)
            record["confirmation"] = now["notice"]
            ctx.check(state == "on", "the switch was already on and this press turned it off (%s); "
                      "run act=arm again to turn it on" % now["notice"])
    elif what == "load-built":
        ctx.check(before["realization"] == "button", "nothing built is waiting to be loaded (realize "
                  "reads %r): Shift+b would switch load-after-build instead. Nothing was pressed"
                  % before["realize"])
        press(ctx, hand, KEYS[what])
        now = follow(ctx, watch, before, record)
    elif what in ("promote", "revert"):
        ctx.check(before["realization"] in REALIZE_ENDED, "nothing this Builder realized is standing "
                  "(realize reads %r); nothing was pressed" % before["realize"])
        press(ctx, hand, KEYS[what])
        now, passed = answer_of(ctx, watch, before, record, what)
        said = "" if passed else "the owner did not %s: %s" % (what, now["realize"])
    elif what == "load-it":
        now = load_it(ctx, hand, watch, before, record)
    else:
        press(ctx, hand, KEYS[what])
        now = follow(ctx, watch, before, record)
    record.update(after=brief(now), rows=now["rows"], elapsed_s=round(time.monotonic() - started, 2))
    build = record["build"]
    if build and build["outcome"] != "succeeded":
        ctx.produce("output.txt", read_output(ctx, hand).encode("utf-8"))
    ctx.produce("builder.json", json.dumps(record, indent=1).encode())
    ctx.check(passed, said)
    if build and expect != "any":
        ctx.check((build["outcome"] == "succeeded") == (expect == "succeeded"), "op #%d's build %s "
                  "(last: %r; realize: %r)" % (build["op"], "did not succeed" if expect == "succeeded"
                                               else "succeeded", now["last"], now["realize"]))
    real = record["realization"]
    if realize != "any" and (build or what == "load-it"):
        outcome = (real or {}).get("outcome") if (real or {}).get("asked") else "not asked"
        want = {"realized": ("realized", "resolved"), "refused": ("REFUSED", "refused")}[realize]
        ctx.check(outcome in want, "the realization was %s, not %s (realize: %r)"
                  % (outcome, realize, (real or {}).get("detail", now["realize"])))
    parts = ["%s:" % what]
    if build:
        parts.append("op #%d build %s;" % (build["op"], build["outcome"]))
    if real and real.get("asked"):
        parts.append("realization %s -- %s;" % (real.get("outcome"), real.get("detail", "")))
    elif build:
        parts.append("realization not asked;")
    if record.get("confirmation"):
        parts.append("said %r;" % record["confirmation"])
    if not build and not real and not record.get("confirmation"):
        parts.append("last %r; realize %r;" % (now["last"], now["realize"]))
    return "%s (%.1fs)" % (" ".join(parts), record["elapsed_s"])
