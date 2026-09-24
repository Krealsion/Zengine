# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/builder -- drive Workshop's Builder pane as a maker would and wait for what it did:
choose a recipe, then build it, build what the project waits on, arm load-after-build, load what
was built, promote or revert the loaded image, or add the recipe's artifact to the load plan with
a role. loom-tool.json describes the acts.

TWO ROWS ARE THE EVIDENCE. `last` is the build tool's word about its latest operation (a number,
and how it ended); `realize` is the realization owner's word about offering the product to the
project. A build waits for a NEW operation number to end on `last`; an act that loads then waits
for `realize` to leave its previous words. Every distinct pair of rows seen on the way is kept in
builder.json, so the spelling of each state is evidence rather than an assumption here. A build
that worked and a load that was refused are two answers: both are reported, and `expect` says
which build outcome passes.

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
WORKING = ("running", "queued", "starting", "building", "configuring", "compiling", "asked",
           "offered")  # measured: realize reads `asked`, then `offered`, then `realized -- weave #N`
KEYS = {"build": "b", "frontier": "f", "arm": "shift+b", "load-built": "shift+b",
        "promote": "shift+p", "revert": "shift+r"}


def view_rows(hand):
    view = painted(hand, *BUILDER)
    return [r["text"] for r in view["rows"]] if view else []


def labelled(rows):
    return dict((r[:9].strip(), r[9:].strip()) for r in rows if r[:9].strip() in LABELS)


def op_of(text):
    found = re.search(r"op #(\d+)", text or "")
    return int(found.group(1)) if found else 0


def busy(text):
    """A row is working while its FIRST word is a working state ("running -- op #8", "asked"):
    a settled sentence can contain such a word ("... the image weave #26 is running now")."""
    return (text or "").split(" ")[0].strip(",").lower() in WORKING


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


def run(ctx):
    what = ctx.inputs["act"]
    ctx.check(what in KEYS or what in ("load-it", "look"), "act must be one of %s, load-it or look"
              % ", ".join(sorted(KEYS)))
    seconds = float(ctx.inputs.get("seconds", 300))
    hand = Hand(ctx, ctx.inputs["link"])
    started = time.monotonic()
    before = labelled(calm(ctx, hand))
    keys_into(ctx, hand)
    if ctx.inputs.get("recipe"):
        choose(ctx, hand, ctx.inputs["recipe"])
    before = labelled(view_rows(hand))
    record = {"act": what, "before": before, "seen": []}
    if what == "load-it":
        ctx.check(bool(ctx.inputs.get("role")), "load-it needs the role the artifact will hold")
        press(ctx, hand, "o")
        hand.inject([moment(ctx, "TextEntered", text=ctx.inputs["role"])])
        press(ctx, hand, "enter")
    elif what != "look":
        press(ctx, hand, KEYS[what])
    builds = what in ("build", "frontier", "load-built")
    loads = what in ("frontier", "load-built", "promote", "revert") or (
        what == "build" and "load-after-build off" in " ".join(view_rows(hand)))
    placed = what == "load-it"  # `o` ends when the project names the artifact: waiting, or loaded
    end = started + seconds
    now = before
    realize_worked = False  # a reload can end on the very words the previous one did
    while what != "look" and time.monotonic() < end:
        rows = view_rows(hand)
        now = labelled(rows)
        pair = [now.get("last", ""), now.get("realize", "")]
        if pair not in record["seen"]:
            record["seen"].append(pair)
        busy_now = busy(now.get("realize"))
        realize_worked = realize_worked or busy_now
        built = op_of(now.get("last")) > op_of(before.get("last")) and not busy(now.get("last"))
        realized = not busy_now and (realize_worked or now.get("realize", "") != before.get("realize", ""))
        if placed and (now.get("project", "").startswith("waiting " + ctx.inputs.get("recipe", ""))
                       or (not busy_now and now.get("realize", "") != before.get("realize", ""))):
            break
        if (not builds or built) and (not loads or realized or (builds and "FAILED" in now.get("last", ""))):
            break
        time.sleep(0.25)
    record.update(after=now, rows=view_rows(hand), elapsed_s=round(time.monotonic() - started, 2))
    finished = what == "look" or (not builds or op_of(now.get("last")) > op_of(before.get("last")))
    succeeded = "succeeded" in now.get("last", "")
    if builds and finished and not succeeded:
        ctx.produce("output.txt", read_output(ctx, hand).encode("utf-8"))
    ctx.produce("builder.json", json.dumps(record, indent=1).encode())
    ctx.check(finished, "the Builder's `last` row did not show a new operation ending within %gs "
              "(it reads %r); the build may still be running" % (seconds, now.get("last")))
    expect = ctx.inputs.get("expect", "succeeded")
    if builds and expect != "any":
        ctx.check(succeeded == (expect == "succeeded"), "the build %s (last: %r; realize: %r)" % (
            "did not succeed" if expect == "succeeded" else "succeeded", now.get("last"), now.get("realize")))
    return "%s: last %r; realize %r (%.1fs)" % (what, now.get("last"), now.get("realize"),
                                               record["elapsed_s"])
