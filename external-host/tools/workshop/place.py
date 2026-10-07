# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/place -- author where panes sit and how big they are by typing the numbers into Info,
as a weaver would: choose the pane's row in Info's list, Return to make it Info's subject, Tab to
its rows, and for each of X, Y, Width and Height: choose its row, Return, the value, Return.
loom-tool.json describes the input.

EVERY ROW IS CHOSEN BY THE NAME INFO GIVES IT: a pane's by its reference, `pane:<office>/<pane>`,
and a property's by its label, `property:Width`. A row is read back by that name too.

WORKSHOP WRITES IT. Info sends the finished text; Workshop writes it through the desk's own door
and reseats the desk. The tool then reads the AUTHORED row back and fails unless it shows the
value typed. A refused value leaves the authored one where it was, and the run says which.

UNITS ARE THE FACE'S. A window takes pixels ("120" means 120 px), a terminal cells. X and Y are
one place, measured from the room's top-left -- writing either sets both -- so X is written
before Y. The placement lands in the layout you are on and returns with the next launch's desk;
`s` in Layouts writes a Setup file."""
import json
import time

from act import act as step, pane_ref, part_of, select_row, texts, words_now
from hand import Hand, on_row
from workshop_steps import chord_moments, clear_moments, moment

INFO = ("zengine.info", "info")
INFO_ROW = "pane:zengine.info/info"
FIELDS = (("x", "X"), ("y", "Y"), ("width", "Width"), ("height", "Height"))


def rows_now(hand):
    return texts(words_now(hand, *INFO))


def listed_name(ctx, hand, name):
    """What the desk calls the pane `name` names -- the subject line Info paints for it."""
    office, pane = pane_ref(name)
    on = [p for p in hand.desk()["panes"] if p["provider"] == office and p["pane"] == pane]
    ctx.check(on, "place: the desk holds no pane %s" % name[len("pane:"):])
    return on[0]["name"]


def list_marked(view):
    """Whether Info marks a row of its pane list -- the keys are on the list, not on the subject: a
    word starting with `>` stands on a row named `pane:`, as a text pane's row and its word share
    one place and a canvas pane's run stands inside its row's rectangle."""
    if not view:
        return False
    listed = [p for p in view.get("parts", []) if p["name"].startswith("pane:")]
    return any(w["text"].startswith(">") and any(on_row(w, p) for p in listed)
               for w in view["words"])


def choose(ctx, hand, name):
    step(ctx, hand, "into", {"into": list(INFO) + ["PANES"]})
    if not list_marked(words_now(hand, *INFO)):
        hand.inject(chord_moments(ctx, "tab"))  # the keys were on the subject's rows
    select_row(ctx, hand, *INFO, name)
    hand.inject(chord_moments(ctx, "enter"))
    subject = "PANE " + listed_name(ctx, hand, name)
    end = time.monotonic() + 5
    while subject not in rows_now(hand):
        ctx.check(time.monotonic() < end, "Info did not take %s as its subject" % name)
        time.sleep(0.1)
    hand.inject(chord_moments(ctx, "tab"))


def field(hand, label):
    """The property row named for `label`, without its cursor mark, or None while it is not drawn:
    the row's own characters."""
    part = part_of(words_now(hand, *INFO), "property:" + label)
    return part["text"][1:] if part is not None else None


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
    select_row(ctx, hand, *INFO, "property:" + label)
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
    ctx.check(isinstance(panes, list) and panes and all(isinstance(p, dict) for p in panes),
              "panes is a JSON list of {\"pane\": \"pane:<office>/<pane>\", \"x\": .., \"y\": .., "
              "\"width\": .., \"height\": ..}")
    for p in panes:
        pane_ref(p.get("pane"))  # a pane named by anything but its row's name, before any contact
    hand = Hand(ctx, ctx.inputs["link"])
    done = []
    try:
        for p in panes:
            ctx.step("place " + p["pane"])
            choose(ctx, hand, p["pane"])
            placed = {"pane": p["pane"]}
            for key, label in FIELDS:
                if key in p:
                    if key == "y" and p["pane"] == INFO_ROW and int(str(p["y"]).rstrip("px")) > 0:
                        # INFO PLACES ITSELF LAST, FROM THE ROOM'S TOP. Moving it down before it is
                        # resized can run its old height past the room's bottom, and Workshop stops
                        # describing the very pane these keys are typed into.
                        write(ctx, hand, "Y", "0")
                        continue
                    placed[key] = write(ctx, hand, label, str(p[key]))
            if p["pane"] == INFO_ROW and "y" in p and int(str(p["y"]).rstrip("px")) > 0:
                placed["y"] = write(ctx, hand, "Y", str(p["y"]))
            done.append(placed)
    finally:
        ctx.produce("place.json", json.dumps(done, indent=1).encode())
    return "placed %d pane(s): %s" % (len(done), ", ".join(p["pane"] for p in done))
