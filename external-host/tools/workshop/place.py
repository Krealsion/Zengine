# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/place -- author where panes sit and how big they are by typing the numbers into Info,
as a maker would: choose the pane in Info's list, Return to make it Info's subject, Tab to its
rows, and for each of X, Y, Width and Height: Return, the value, Return. loom-tool.json describes
the input.

WORKSHOP WRITES IT. Info sends the finished text; Workshop writes it through the desk's own door
and reseats the desk. The tool then reads the AUTHORED row back and fails unless it shows the
value typed. A refused value leaves the authored one where it was, and the run says which.

UNITS ARE THE FACE'S. A window takes pixels ("120" means 120 px), a terminal cells. X and Y are
one place -- writing either sets both -- so X is written before Y. The placement lands in the
layout you are on and returns with the next launch's desk; `s` in Layouts writes a Setup file."""
import json
import time

from act import act as step, painted
from hand import Hand
from workshop_steps import chord_moments, clear_moments, moment

INFO = ("zengine.info", "info")
FIELDS = (("x", "X"), ("y", "Y"), ("width", "Width"), ("height", "Height"))


def rows_now(hand):
    view = painted(hand, *INFO)
    return [r["text"] for r in view["rows"]] if view else []


def split(rows):
    """(list rows, subject rows): Info paints its pane list, then PANE <name> or PROPERTIES."""
    for i, text in enumerate(rows):
        if i and (text.startswith("PANE ") or text == "PROPERTIES"):
            return rows[:i], rows[i:]
    return rows, []


def walk(ctx, hand, found, what):
    """Press Down, then Up, until `found(rows)`; rows that stop changing are the list's end."""
    rows = []
    for key in ("down", "up"):
        previous = None
        for _ in range(48):
            rows = rows_now(hand)
            if found(rows):
                return rows
            if rows == previous:
                break
            previous = rows
            hand.inject(chord_moments(ctx, key))
    ctx.produce("failed-rows.json", json.dumps(rows, indent=1).encode())
    ctx.fail("Info never chose %s" % what)


def choose(ctx, hand, name):
    step(ctx, hand, "into", {"into": list(INFO) + ["PANES"]})
    listed, _ = split(rows_now(hand))
    if not any(r.startswith(">") for r in listed[1:]):
        hand.inject(chord_moments(ctx, "tab"))  # the keys were on the subject's rows
    walk(ctx, hand, lambda rows: any(r.startswith(">") and r[2:].startswith(name + " -- ")
                                     for r in split(rows)[0]), "the pane %r in its list" % name)
    hand.inject(chord_moments(ctx, "enter"))
    end = time.monotonic() + 5
    while "PANE " + name not in split(rows_now(hand))[1][:1]:
        ctx.check(time.monotonic() < end, "Info did not take %r as its subject" % name)
        time.sleep(0.1)
    hand.inject(chord_moments(ctx, "tab"))


def field(hand, label):
    """The subject row for `label`, without its cursor mark, or None while it is not painted."""
    rows = [r[1:] for r in split(rows_now(hand))[1] if r[1:].startswith(label + " ")]
    return rows[0] if rows else None


def settle(ctx, hand, label, wanted, what):
    end = time.monotonic() + 5
    while True:
        row = field(hand, label)
        if row is not None and wanted(row.split()[1:]):
            return row
        if time.monotonic() > end:
            ctx.produce("failed-rows.json", json.dumps(rows_now(hand), indent=1).encode())
            ctx.fail("%s: Info never showed %s (it reads %r)" % (label, what, row))
        time.sleep(0.05)


def write(ctx, hand, label, value):
    """One field, one key at a time: the draft opens on Return (a round trip at Info), and keys
    batched behind that Return reached it before the draft did. A draft paints only the typed
    text; the written value is painted with its unit ("24 px"), which is what the check waits for."""
    walk(ctx, hand, lambda rows: any(r.startswith(">" + label + " ") for r in split(rows)[1]),
         "the %s row" % label)
    number = value.strip().rstrip("px").strip()
    hand.inject(chord_moments(ctx, "enter"))
    hand.inject(clear_moments(ctx))
    settle(ctx, hand, label, lambda words: words == [], "an empty draft")
    hand.inject([moment(ctx, "TextEntered", text=value)])
    settle(ctx, hand, label, lambda words: words == [value.strip()], "the typed %r" % value)
    hand.inject(chord_moments(ctx, "enter"))
    return settle(ctx, hand, label, lambda words: len(words) >= 2 and words[0] == number,
                  "%s written with its unit" % number)


def run(ctx):
    panes = json.loads(ctx.inputs["panes"])
    ctx.check(isinstance(panes, list) and panes and
              all(isinstance(p, dict) and p.get("pane") for p in panes),
              "panes is a JSON list of {\"pane\": name, \"x\": .., \"y\": .., \"width\": .., \"height\": ..}")
    hand = Hand(ctx, ctx.inputs["link"])
    done = []
    try:
        for p in panes:
            ctx.step("place " + p["pane"])
            choose(ctx, hand, p["pane"])
            placed = {"pane": p["pane"]}
            for key, label in FIELDS:
                if key in p:
                    if key == "y" and p["pane"] == "Info" and int(str(p["y"]).rstrip("px")) > 24:
                        # INFO PLACES ITSELF LAST, FROM THE TOP. Moving it down before it is resized
                        # can run its old height past the room's bottom, and Workshop stops
                        # describing the very pane these keys are typed into.
                        write(ctx, hand, "Y", "24")
                        continue
                    placed[key] = write(ctx, hand, label, str(p[key]))
            if p["pane"] == "Info" and "y" in p and int(str(p["y"]).rstrip("px")) > 24:
                placed["y"] = write(ctx, hand, "Y", str(p["y"]))
            done.append(placed)
    finally:
        ctx.produce("place.json", json.dumps(done, indent=1).encode())
    return "placed %d pane(s): %s" % (len(done), ", ".join(p["pane"] for p in done))
