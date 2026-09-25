# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/builder -- press the Builder pane's keys as a maker would, and follow what the Builder
itself says became of the press: choose a recipe, then build it, build what the project waits on,
arm load-after-build, load what was built, promote or revert the loaded image, or add the recipe's
artifact to the load plan with a role. loom-tool.json describes the acts.

THE BUILDER'S OWN WORDS ARE THE EVIDENCE, AS TYPED OBSERVATIONS. Before any key is pressed the run
subscribes, through the link, to the Builder's publications (``ctx.observe``: Loom's observation
relay, which Workshop mounts beside its guest door; the guest's row must list ``zengine.builder``
``BuildAsked`` v1 and ``BuildStatus`` v4 under ``observe``):

  BuildAsked   what the Builder did with ONE ask: took it as ask number N, or refused it and why
  BuildStatus  its whole picture: which operation ask N became, where that build stands and, for a
               BUILD & REALIZE, where its realization stands -- two answers kept apart

WHOSE ASK IT IS. Every press is settled, so everything it set in motion synchronously has arrived
before the press returns, marked with this run's own correlation for the press (``cause``). The
ask this run made is the BuildAsked its press caused -- never the first one to arrive -- and its
operation is the one the Builder's statuses name beside that ask's number (the Builder follows one
build at a time). A status about a later ask before this one's ending is SUPERSEDED: its ending was
not seen, and another build's is never taken for it.

WHAT COMPLETES EACH ACT. A build, the frontier, load-built and a load-it that loads now are one
ask: taken or refused, then its build ENDS (succeeded, FAILED, NO ARTIFACT, did not start, unknown
recipe) and, when it asked for realization, its realization ENDS (realized or REFUSED). Running,
asked and offered are never an ending. Promote and revert end on the realization owner's own
sentence folded into a status. Arm is the pane's own switch, read from the pane.

THE PANE IS THE ACTION PATH, NOT THE OBSERVER. Its keys are how a maker asks, so it must be visible,
uncovered and hold the keys at the press; the input session is given back right after it. From
then on the pane may be covered, closed or resized: nothing here reads it to learn an ending. It is
read for the pane's own acts (choosing a recipe, the switch, the load-it line), once before the
press as a picture for the record, and for a failed build's words (the output reader, ``l``).

UNRESOLVED IS NEITHER FAILED NOR PASSED. When the bounded wait ends first -- or the observation
ends (revoked, or lost with the link) -- the run fails as UNRESOLVED, naming the ask and the
operation as far as they are known; the Builder carries on. ``act=look op=N`` follows that
operation again and presses nothing. No key is ever pressed a second time."""
import json
import re
import time

from act import painted
from hand import Hand
from workshop_steps import chord_moments, moment

BUILDER = ("zengine.builder-pane", "builder")
PRODUCER = "zengine.builder"
WATCHED = [("BuildAsked", 1), ("BuildStatus", 4)]
LABELS = ("recipe", "project", "last", "exit", "ran", "realize", "said")
KEYS = {"build": "b", "frontier": "f", "arm": "shift+b", "load-built": "shift+b",
        "promote": "shift+p", "revert": "shift+r"}
ACTS = tuple(KEYS) + ("load-it", "look")
TAKE_SECONDS = 15.0  # a key the Builder answers takes its ask in one delivery; this is slack

# The Builder's own numbers (builder/vocabulary.hpp), in the words its pane uses.
OUTCOME = {0: "not built yet", 1: "asked", 2: "succeeded", 3: "FAILED", 4: "did not start",
           5: "unknown recipe", 6: "running", 7: "NO ARTIFACT"}
STILL_GOING = (1, 6)
REALIZATION = {0: "not asked", 1: "asked", 2: "offered", 3: "realized", 4: "REFUSED"}
REALIZE_ENDED = (3, 4)


class Record(dict):
    """builder.json's content, and the names its files take in this run: a monitor performing
    an act in-process names them after itself, so its own record is never written over."""
    file = "builder.json"
    output = "output.txt"


def keep(ctx, record):
    ctx.produce(record.file, json.dumps(record, indent=1).encode())


# ---- the pane: the action path, and a picture for the record ------------------------------------

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
    """One picture of the pane's rows: the pane's own state (the switch, its notice, the load-it
    line) and, for the record, what it painted -- never how a build ended."""
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
    """Press a chord, settled; the Input owner's answer (its correlation names the press)."""
    return hand.inject(chord_moments(ctx, chord))


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


# ---- the Builder's own words --------------------------------------------------------------------

class Words:
    """The subscription, and every word it handed over kept in the record, in order."""

    def __init__(self, ctx, sub, record, seconds):
        self.ctx, self.sub, self.record = ctx, sub, record
        self.began = time.monotonic()
        self.end = self.began + seconds
        self.seconds = seconds
        self.kept = record.setdefault("words", [])
        self.backlog = []   # taken from the subscription, not yet judged
        self.times = record.setdefault("seconds", {})

    def mark(self, what):
        self.times.setdefault(what, round(time.monotonic() - self.began, 3))

    def arrived(self):
        """What has arrived now, without waiting, in order."""
        items = self.backlog + self.sub.drain()
        self.backlog = []
        for i in items:
            self.kept.append(i.record())
        return items

    def next(self, bound=None):
        """The next word, or None when the wait (or the smaller `bound`) ran out."""
        if self.backlog:
            return self.backlog.pop(0)
        end = min(self.end, time.monotonic() + bound) if bound else self.end
        item = self.sub.next(max(0.0, end - time.monotonic()))
        if item is not None:
            self.kept.append(item.record())
        return item


def unresolved(ctx, record, stage, why, ask=None, op=0, last=None):
    """The wait ended first, or the observation did: named, and recoverable."""
    record["unresolved"] = {"stage": stage, "why": why, "ask": ask, "op": op or None,
                            "last_status": last}
    keep(ctx, record)
    again = (" `act=look op=%d` follows it again and presses nothing." % op) if op else ""
    ctx.fail("UNRESOLVED: %s -- %s (ask %s, operation %s). The Builder carries on with it; nothing "
             "was pressed twice.%s" % (stage, why, ask if ask is not None else "unknown",
                                       ("#%d" % op) if op else "not numbered", again))


def superseded(ctx, record, why, ask, op):
    record["superseded"] = {"why": why, "ask": ask, "op": op or None}
    keep(ctx, record)
    ctx.fail("SUPERSEDED: %s. Another build's ending is never taken for this run's; %s. Nothing was "
             "pressed twice." % (why, ("op #%d's own words are in the output reader's older builds"
                                       % op) if op else "this ask was not seen to become an operation"))


def the_ask(ctx, words, press_answer, record):
    """THE ASK THIS PRESS MADE: the one BuildAsked its settled press caused."""
    corr = press_answer.correlation
    items = words.arrived()
    asked = [i for i in items if i.kind == "observed" and i.shape == "BuildAsked" and i.cause == corr]
    words.backlog = [i for i in items if not (i.kind == "observed" and i.shape == "BuildAsked"
                                              and i.cause == corr)]
    ctx.check(len(asked) <= 1, "one press made %d asks the Builder heard; which is this run's cannot "
              "be told" % len(asked))
    if not asked:
        return None
    a = asked[0]
    words.mark("ask")
    record["ask"] = {"number": a["ask"], "taken": a["taken"], "recipe": a["recipe"],
                     "realize": a["realize"], "refusal": a["refusal"], "cause": corr,
                     "seq": a.seq, "far_delivery": a.delivery, "far_published_in": a.published_in,
                     "attributed_by": "BuildAsked caused by this press (correlation %d)" % corr}
    return a


def follow(ctx, words, record, ask=None, op=0):
    """ONE ASK'S OPERATION, from the statuses beside its number to its build's and its
    realization's endings. With `op` and no ask (look), the operation is named by the caller."""
    number = ask["ask"] if ask is not None else None
    realize = bool(ask["realize"]) if ask is not None else None
    build = realization = last = None
    stage = "the build has not ended"
    while True:
        item = words.next()
        if item is None:
            unresolved(ctx, record, stage, "the wait of %.0fs ran out" % words.seconds, number, op, last)
        if item.kind == "gap":
            record.setdefault("gaps", []).append(item.record())
            continue  # a status is the whole picture: the next one about this ask still decides
        if item.kind == "ended":
            unresolved(ctx, record, stage, "the observation ended (%s): %s" % (item.how, item.reason),
                       number, op, last)
        if item.shape != "BuildStatus":
            continue  # another ask's BuildAsked: this run's is already known
        s = item.fields
        if number is not None:
            if s["builds"] < number:
                continue  # a republished picture from before this ask
            if s["builds"] > number:
                superseded(ctx, record, "the Builder took ask %d before ask %d's ending was seen"
                           % (s["builds"], number), number, op)
        elif s["op"] and s["op"] != op:
            if s["op"] > op:
                superseded(ctx, record, "op #%d followed op #%d before its ending was seen"
                           % (s["op"], op), None, op)
            continue
        if s["outcome"] == 5:
            # ANOTHER ASK'S REFUSAL, NOT THIS BUILD'S ENDING: the Builder judges an unknown recipe
            # before its one-at-a-time rule, and says so in the one outcome field while a build
            # runs. A taken ask's recipe was known, so its build never ends this way.
            record.setdefault("set_aside", []).append({"seq": item.seq, "why": s["detail"]})
            continue
        last = {"seq": item.seq, "op": s["op"], "outcome": OUTCOME.get(s["outcome"], s["outcome"]),
                "realization": REALIZATION.get(s["realization"], s["realization"]),
                "detail": s["detail"], "realized_detail": s["realized_detail"]}
        if s["op"] and not op:
            op = s["op"]
            words.mark("operation")
            record["operation"] = {"op": op, "seq": item.seq, "cause": item.cause,
                                   "named_by": "the status beside ask %d" % number}
        if realize is None:
            realize = bool(s["realize"])
        if build is None and s["outcome"] not in STILL_GOING and s["outcome"] != 0:
            build = OUTCOME.get(s["outcome"], str(s["outcome"]))
            words.mark("build")
            record["build"] = {"op": op, "outcome": build, "status": s["status"], "seq": item.seq,
                               "far_delivery": item.delivery, "detail": s["detail"]}
            stage = "op #%d's build ended %s and its realization is pending" % (op, build)
        if build is not None and (not realize or s["realization"] in REALIZE_ENDED):
            if realize:
                realization = REALIZATION[s["realization"]]
                words.mark("realization")
            record["realization"] = {"asked": realize, "op": op, "outcome": realization,
                                     "detail": s["realized_detail"],
                                     "default_image": s["default_image"],
                                     "seq": item.seq if realize else None}
            return build, realization


def owner_answer(ctx, words, press_answer, record, what):
    """Promote and revert start no build: the realization owner's sentence is folded into a status.
    The first status after the press that carries it is the answer; a status caused by the press
    is preferred, and one about a later ask is not this answer."""
    corr = press_answer.correlation
    word = "promoted:" if what == "promote" else "reverted:"
    while True:
        item = words.next()
        if item is None:
            unresolved(ctx, record, "no answer to %s arrived" % what,
                       "the wait of %.0fs ran out" % words.seconds)
        if item.kind != "observed" or item.shape != "BuildStatus":
            if item.kind == "ended":
                unresolved(ctx, record, "no answer to %s arrived" % what,
                           "the observation ended (%s): %s" % (item.how, item.reason))
            continue
        s = item.fields
        if word not in s["realized_detail"] and "could not be" not in s["realized_detail"] \
                and item.cause != corr:
            continue
        done = word in s["realized_detail"] and (what == "revert" or s["default_image"])
        words.mark("answer")
        record["realization"] = {"asked": True, "op": s["op"], "outcome": REALIZATION[s["realization"]],
                                 "detail": s["realized_detail"], "default_image": s["default_image"],
                                 what: done, "caused_by_press": item.cause == corr, "seq": item.seq}
        return done, s


def load_it(ctx, hand, record, role):
    """`o`, the role and Return; then the pane's own sentence about the plan row. Returns the press
    of Return and whether it loads the built product now."""
    before = observe(hand)
    stem = before["recipe"].split("->")[-1].strip().split(" ")[0]
    ctx.check(stem, "no recipe is chosen (the recipe row reads %r)" % before["recipe"])
    press(ctx, hand, "o")
    end = time.monotonic() + TAKE_SECONDS
    while True:
        now = observe(hand)
        if any(r.startswith("role for %s>" % stem) for r in now["rows"]):
            break
        ctx.check(now["notice"] == before["notice"] or "type the role" in now["notice"],
                  "the Builder did not open a role line for %s: %s" % (stem, now["notice"]))
        ctx.check(time.monotonic() < end, "`o` opened no role line for %s" % stem)
        time.sleep(0.2)
    hand.inject([moment(ctx, "TextEntered", text=role)])
    enter = press(ctx, hand, "enter")
    loaded, loading = "loaded `%s` as " % stem, "loading `%s` now" % stem
    end = time.monotonic() + TAKE_SECONDS
    while True:
        n = observe(hand)["notice"]
        if n.startswith(loaded) or n.startswith(loading) or (
                n and "type the role" not in n and n != before["notice"]):
            break
        ctx.check(time.monotonic() < end, "the Builder did not answer the role for %s" % stem)
        time.sleep(0.2)
    record["confirmation"] = n
    if n.startswith(loading):
        record["plan_row"] = "written; the built product is loaded now"
        return enter, True
    ctx.check(n.startswith(loaded), "the Builder did not add %s to the plan: %s" % (stem, n))
    detail = n[len(loaded):].split(" -- ", 1)[-1]
    state = next((w for w in ("resolved", "pending", "loading", "refused", "authored behind")
                  if detail.startswith(w)), "said")
    record["plan_row"] = "written"
    record["realization"] = {"asked": True, "outcome": {"authored behind": "pending"}.get(state, state),
                             "detail": detail}
    return enter, False


def ask_made(ctx, hand, words, press_answer, record):
    """The ask a press made, or the pane's own words for why it made none."""
    a = the_ask(ctx, words, press_answer, record)
    if a is None:
        notice = observe(hand)["notice"] if hand.open else ""
        record["ask"] = {"number": None, "taken": False, "cause": press_answer.correlation,
                         "pane_notice": notice}
        keep(ctx, record)
        ctx.fail("the Builder was asked nothing by this press (no BuildAsked it caused arrived "
                 "before the press settled)%s" % (": the pane says %r" % notice if notice else ""))
    if not a["taken"]:
        record["build"] = {"op": None, "outcome": "not taken", "detail": a["refusal"]}
    return a


def run(ctx):
    return perform(ctx, ctx.inputs)


def perform(ctx, inputs, name="builder"):
    """ONE ACT, AS THE TOOL DOES IT, for this run or for a monitor that builds before it plays:
    `inputs` are the tool's (loom-tool.json), and `name` names the record (`<name>.json`) and a
    failed build's words (`output.txt` for the tool, `<name>-output.txt` otherwise). Returns the
    summary; fails the run as the tool would."""
    what = inputs["act"]
    ctx.check(what in ACTS, "act must be one of %s" % ", ".join(ACTS))
    expect = inputs.get("expect", "succeeded")
    ctx.check(expect in ("succeeded", "failed", "any"), "expect is succeeded, failed or any")
    realize = inputs.get("realize", "any")
    ctx.check(realize in ("realized", "refused", "any"), "realize is realized, refused or any")
    ctx.check(what != "load-it" or inputs.get("role"), "load-it needs the role the artifact will hold")
    op = int(inputs.get("op", 0) or 0)
    seconds = float(inputs.get("seconds", 300))
    link = inputs["link"]
    started = time.monotonic()
    record = Record(act=what, build=None, realization=None)
    if name != "builder":
        record.file, record.output = name + ".json", name + "-output.txt"
    # SUBSCRIBED BEFORE ANYTHING IS PRESSED: every word the Builder says from here on is kept.
    sub = ctx.observe(PRODUCER, WATCHED, via=link, latest=["BuildStatus"],
                      label="workshop/builder %s (run %s)" % (what, ctx.name))
    words = Words(ctx, sub, record, seconds)
    record["subscription"] = {"subscription": sub.subscription, "relay": sub.relay,
                              "holder": sub.holder, "incarnation": sub.incarnation,
                              "window": sub.window}
    hand = Hand(ctx, link)
    calm(ctx, hand)
    keys_into(ctx, hand)
    if inputs.get("recipe") and what != "look":
        choose(ctx, hand, inputs["recipe"])
    before = observe(hand)
    record["before"] = brief(before)
    passed, said, pressed = True, "", None
    if what == "look":
        hand.close()  # nothing is pressed
        ctx.check(op > 0, "look needs the operation to follow (op=N)")
        record["ask"] = {"number": None, "attributed_by": "look: op #%d, named by the caller" % op}
        if before["op"] == op and before["build"] in OUTCOME.values() and \
                before["build"] not in ("asked", "running"):
            record["build"] = {"op": op, "outcome": before["build"],
                               "from": "the pane's row: it had ended before this look subscribed"}
        else:
            follow(ctx, words, record, op=op)
    elif what == "arm":
        if before["armed"]:
            record["confirmation"] = "already on; nothing was pressed"
        else:
            ctx.check(before["realization"] != "button", "a build is waiting to be loaded (realize "
                      "reads %r): Shift+b would load it now, not arm the next one; use act=load-built. "
                      "Nothing was pressed" % before["realize"])
            press(ctx, hand, "shift+b")
            end = time.monotonic() + TAKE_SECONDS
            while True:
                snap = observe(hand)
                # The control strip names the switch's state when it fits; otherwise the pane's
                # notice does, once it has changed from what it said before the press.
                if before["armed"] is not None and snap["armed"] is not None:
                    state = "on" if snap["armed"] else None
                elif snap["notice"] != before["notice"]:
                    state = "on" if snap["notice"].startswith("load after build: on") else (
                        "off" if snap["notice"].startswith("load after build: off") else None)
                else:
                    state = None
                if state is not None:
                    break
                ctx.check(time.monotonic() < end, "the load-after-build switch did not answer")
                time.sleep(0.2)
            record["confirmation"] = snap["notice"]
            ctx.check(state == "on", "the switch was already on and this press turned it off (%s); "
                      "run act=arm again to turn it on" % snap["notice"])
    elif what in ("promote", "revert"):
        ctx.check(before["realization"] in ("realized", "REFUSED"), "nothing this Builder realized "
                  "is standing (realize reads %r); nothing was pressed" % before["realize"])
        pressed = press(ctx, hand, KEYS[what])
        words.mark("press")
        hand.close()
        passed, s = owner_answer(ctx, words, pressed, record, what)
        said = "" if passed else "the owner did not %s: %s" % (what, s["realized_detail"])
    else:
        ask = None
        if what == "load-it":
            pressed, now = load_it(ctx, hand, record, inputs["role"])
            words.mark("press")
            if now:
                ask = ask_made(ctx, hand, words, pressed, record)
        else:
            if what == "load-built":
                ctx.check(before["realization"] == "button", "nothing built is waiting to be loaded "
                          "(realize reads %r): Shift+b would switch load-after-build instead. Nothing "
                          "was pressed" % before["realize"])
            pressed = press(ctx, hand, KEYS[what])
            words.mark("press")
            ask = ask_made(ctx, hand, words, pressed, record)
        # THE INPUT SESSION GOES BACK NOW: following needs no key, and the pane may be covered,
        # closed or resized from here on without this run noticing.
        hand.close()
        if ask is not None and ask["taken"]:
            follow(ctx, words, record, ask=ask)
    record.update(after=None, elapsed_s=round(time.monotonic() - started, 2))
    record["observation"] = sub.summary()
    build = record["build"]
    if build and build.get("outcome") not in ("succeeded", None) and build.get("op"):
        try:
            again = Hand(ctx, link)
            calm(ctx, again)
            keys_into(ctx, again)
            ctx.produce(record.output, read_output(ctx, again).encode("utf-8"))
            again.close()
        except Exception as err:  # the words are the pane's; not reaching them is said, not fatal
            record["output"] = "the output reader was not read: %s: %s" % (type(err).__name__, err)
    keep(ctx, record)
    ctx.check(passed, said)
    if build and expect != "any":
        ctx.check((build["outcome"] == "succeeded") == (expect == "succeeded"), "op #%s's build %s "
                  "(%s)" % (build.get("op"), "did not succeed" if expect == "succeeded" else "succeeded",
                            build.get("detail", "")))
    real = record["realization"]
    if realize != "any" and (build or what == "load-it"):
        outcome = (real or {}).get("outcome") if (real or {}).get("asked") else "not asked"
        want = {"realized": ("realized", "resolved"), "refused": ("REFUSED", "refused")}[realize]
        ctx.check(outcome in want, "the realization was %s, not %s (%s)"
                  % (outcome, realize, (real or {}).get("detail", "")))
    parts = ["%s:" % what]
    if record.get("ask") and record["ask"].get("number"):
        parts.append("ask %d" % record["ask"]["number"])
    if build:
        parts.append("op #%s build %s;" % (build.get("op"), build["outcome"]))
    if real and real.get("asked"):
        parts.append("realization %s -- %s;" % (real.get("outcome"), real.get("detail", "")))
    elif build:
        parts.append("realization not asked;")
    if record.get("confirmation"):
        parts.append("said %r;" % record["confirmation"])
    return "%s (%.1fs)" % (" ".join(parts), record["elapsed_s"])
