# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/act -- act on Workshop through a short list of named steps, checking each by the rows a
pane paints instead of by comparing pictures. loom-tool.json lists the verbs and their arguments.

A step is one maker gesture (press, type, into, click, control) or one observation (expect,
absent, rows, picture, wait). Steps run in order through ONE input session; the first that cannot
be done ends the run naming its index and verb, and nothing after it is sent. Every step is
spelled before any Workshop contact, so a misspelled list costs nothing.

WHAT A ROW PROVES. PaneView is Workshop's own account of what a pane painted, located by the same
measurer that places a press (hand.py). `expect` therefore proves presentation: that a pane
painted a row, not that the owner it paints about finished its work. Pair it with that owner's
evidence when completion matters. A pane Workshop will not describe (closed, unsettled, covered)
is treated as painting nothing while an `expect` waits, and as a failure anywhere else.

WHAT IT CLEANS UP. hand.py registers the input session's close with the run's cleanup, so a
failed step, a bug or a cancellation still gives Workshop's input session back."""
import json
import time
from pathlib import Path

from loom_session.tool import Refused

from hand import Hand
from workshop_steps import chord_moments, moment, picture, png_of

VERBS = ("press", "type", "open", "select", "into", "click", "at", "control", "expect", "absent",
         "rows", "picture", "wait")
GESTURES = ("press", "type", "open", "select", "into", "click", "at", "control")
BUTTONS = {"left": 1, "right": 3}


def run(ctx):
    steps = json.loads(ctx.inputs["steps"])
    ctx.check(isinstance(steps, list) and steps, "steps must be a non-empty JSON list")
    for i, step in enumerate(steps):
        named = [v for v in VERBS if isinstance(step, dict) and v in step]
        ctx.check(len(named) == 1, "step %d names %s; each step names exactly one of %s"
                  % (i, named or "no verb", ", ".join(VERBS)))
    pace = max(0, int(ctx.inputs.get("pace_ms", 0))) / 1000.0
    hand = Hand(ctx, ctx.inputs["link"])
    done = []
    try:
        for i, step in enumerate(steps):
            verb = [v for v in VERBS if v in step][0]
            ctx.step("step %d of %d: %s" % (i + 1, len(steps), verb))
            started = time.monotonic()
            record = {"index": i, "verb": verb, "args": step[verb]}
            record.update(act(ctx, hand, verb, step) or {})
            record["elapsed_ms"] = round((time.monotonic() - started) * 1000, 1)
            done.append(record)
            if pace and verb in GESTURES:
                time.sleep(pace)
    finally:
        ctx.produce("steps.json", json.dumps(done, indent=1).encode())
    return "%d step(s) done: %s" % (len(done), " ".join(r["verb"] for r in done))


def names(value, verb, count):
    ok = isinstance(value, list) and len(value) >= count and all(isinstance(v, str) for v in value)
    if not ok:
        raise ValueError("%s names [provider, pane%s]" % (verb, ", text" if count > 2 else ", text?"))
    return value


def painted(hand, provider, pane):
    """The pane's rows now, or None while Workshop refuses to describe it."""
    try:
        return hand.view(provider, pane)
    except Refused:
        return None


def wait_rows(hand, provider, pane, text, seconds, present=True):
    """Read the pane until a row holds `text` (or, `present` false, until none does)."""
    end = time.monotonic() + seconds
    while True:
        view = painted(hand, provider, pane)
        rows = [r for r in view["rows"] if text in r["text"]] if view else []
        if bool(rows) == present:
            return view, rows
        if time.monotonic() >= end:
            return view, None
        time.sleep(0.2)

def act(ctx, hand, verb, step):
    """Do one step. Returns what it read, for steps.json."""
    arg = step[verb]
    if verb == "press":
        hand.inject(chord_moments(ctx, arg, repeat=int(step.get("repeat", 1))))
        return {}
    if verb == "type":
        ctx.check(isinstance(arg, str) and arg, "type carries non-empty text")
        hand.inject([moment(ctx, "TextEntered", text=arg)])
        return {"bytes": len(arg.encode("utf-8"))}
    if verb == "open":
        return open_pane(ctx, hand, arg, float(step.get("seconds", 10)))
    if verb == "select":
        provider, pane, name = names(arg, verb, 3)[:3]
        return select_row(ctx, hand, provider, pane, name)
    if verb in ("into", "click"):
        # INTO gives a pane the keys by pressing one of its painted rows (the first, or the one
        # holding the text); CLICK presses where the text itself is painted. Both press only what
        # Workshop says is painted now, located by Workshop's own measurer.
        provider, pane = names(arg, verb, 2)[:2]
        text = arg[2] if len(arg) > 2 else ""
        ctx.check(verb == "into" or text, "click names the text to press on")
        view, rows = wait_rows(hand, provider, pane, text, float(step.get("seconds", 10)))
        ctx.check(rows, "%s: %s/%s paints no row holding %r" % (verb, provider, pane, text))
        at = rows[0]
        where = hand.point(provider, pane, at["row"], max(0, at["text"].find(text)), view["picture"])
        button = BUTTONS[step.get("button", "left")]
        hand.inject([moment(ctx, "PointerButton", button=button, pressed=p, x=where["x"],
                            y=where["y"], space=where["space"]) for p in (True, False)])
        return {"row": at["text"]}
    if verb == "at":
        # One painted cell by its row and column in the pane's own lattice (PaneView's rows,
        # counted from 0): for panes whose meaning is a grid rather than a labelled row.
        ok = isinstance(arg, list) and len(arg) == 4 and all(isinstance(v, str) for v in arg[:2]) \
            and all(isinstance(v, int) for v in arg[2:])
        ctx.check(ok, "at names [provider, pane, row, column]")
        view = painted(hand, arg[0], arg[1])
        ctx.check(view is not None, "at: Workshop does not describe %s/%s now" % (arg[0], arg[1]))
        where = hand.point(arg[0], arg[1], arg[2], arg[3], view["picture"])
        button = BUTTONS[step.get("button", "left")]
        hand.inject([moment(ctx, "PointerButton", button=button, pressed=p, x=where["x"],
                            y=where["y"], space=where["space"]) for p in (True, False)])
        return {"x": where["x"], "y": where["y"]}
    if verb == "control":
        provider, pane, label = names(arg, verb, 3)[:3]
        hand.control(provider, pane, label)
        return {}
    if verb in ("expect", "absent"):
        provider, pane, text = names(arg, verb, 3)[:3]
        seconds = float(step.get("seconds", 10))
        view, rows = wait_rows(hand, provider, pane, text, seconds, verb == "expect")
        if rows is None:
            ctx.produce("failed-step-rows.json", json.dumps(
                [r["text"] for r in view["rows"]] if view else "not described", indent=1).encode())
            ctx.fail("%s: %s/%s %s %r within %gs" % (verb, provider, pane, "never painted"
                     if verb == "expect" else "still paints", text, seconds))
        return {"matched": [r["text"] for r in rows]}
    if verb == "rows":
        provider, pane = names(arg, verb, 2)[:2]
        view = painted(hand, provider, pane)
        ctx.check(view is not None, "rows: Workshop does not describe %s/%s now" % (provider, pane))
        kept = step.get("as", pane.replace(".", "-"))
        ctx.produce(kept + ".json", json.dumps({"picture": view["picture"],
                    "rows": [r["text"] for r in view["rows"]]}, indent=1).encode())
        return {"rows": len(view["rows"]), "picture": view["picture"]}
    if verb == "picture":
        captured, data = picture(ctx, hand.link, arg)
        kept = {"frame": captured["frame"]}
        if (step.get("png") or step.get("save")) and captured["format"] == "image/bmp":
            png = png_of(data, step.get("crop"))
            ctx.produce(arg + ".png", png)
            if step.get("save"):
                # A picture for a document or a report: written where the step names, nowhere else.
                path = Path(step["save"])
                ctx.check(path.is_absolute() and path.suffix == ".png", "save names an absolute .png path")
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(png)
                kept["saved"] = str(path)
        return kept
    time.sleep(float(arg))
    return {}



def select_row(ctx, hand, provider, pane, name):
    """Move a list's cursor -- the row the pane paints with a leading '>' -- with Down and Up until
    it names exactly `name` (the row's text after its two-column marker), and stop there. A row
    naming it that is already painted sets the direction; otherwise Down to the list's end, then
    Up. The walk is bounded: a direction ends where the marked row stops moving. A name no row
    carries, or more than one row carries, fails with the rows the pane painted, and nothing is
    pressed after that. Nothing is chosen by position: whatever else the list holds, and in
    whatever order, the marked row is read back before the step ends."""
    seen, presses = [], 0

    def look():
        view = painted(hand, provider, pane)
        ctx.check(view is not None, "select: Workshop does not describe %s/%s now" % (provider, pane))
        rows = [r["text"] for r in view["rows"]]
        chosen = [i for i, r in enumerate(rows) if r.startswith(">")]
        named = [i for i, r in enumerate(rows) if r[:1] in (">", " ") and r[2:].rstrip() == name]
        if len(named) > 1:
            ctx.produce("failed-step-rows.json", json.dumps(rows, indent=1).encode())
            ctx.fail("select: %d rows of %s/%s are named %r" % (len(named), provider, pane, name))
        return rows, (chosen[0] if chosen else None), (named[0] if named else None)

    rows, at, target = look()
    if target is not None and at is not None:
        order = ("down", "up") if target > at else ("up", "down")
    else:
        order = ("down", "up")
    for key in order:
        previous = None
        for _ in range(256):
            rows, at, target = look()
            if at is not None and at == target:
                return {"chosen": rows[at], "presses": presses}
            seen += [rows[at]] if at is not None else []
            if at is None or rows[at] == previous:
                break
            previous = rows[at]
            hand.inject(chord_moments(ctx, key))
            presses += 1
    ctx.produce("failed-step-rows.json", json.dumps(rows, indent=1).encode())
    ctx.fail("select: %s/%s marks no row named %r (it marked %s)"
             % (provider, pane, name, ", ".join(repr(r) for r in sorted(set(seen))) or "no row"))


def open_pane(ctx, hand, name, seconds):
    """Open, or go to, the pane the Pane Manager lists as `name`: Ctrl+P, then Down (and, at the
    list's end, Up) until the chosen row -- the one painted with a leading '>' -- ends with the
    name, then Return. The Pane Manager has no search, so the walk is bounded; a name it does not
    list fails with the rows it did paint."""
    manager = ("zengine.desktop", "launcher")
    view = painted(hand, *manager)
    if view is None or not view["rows"]:
        # Ctrl+P toggles: it opens a closed Pane Manager and closes an open one -- including one
        # that is open but covered, which Workshop does not describe. So when the first press
        # brings no describable Pane Manager it may have hidden a covered one: press it again.
        hand.inject(chord_moments(ctx, "ctrl+p"))
        view, rows = wait_rows(hand, *manager, "PANES", 1.5)
        if not rows:
            hand.inject(chord_moments(ctx, "ctrl+p"))
            view, rows = wait_rows(hand, *manager, "PANES", seconds)
        ctx.check(rows, "the Pane Manager did not open")
    else:
        # Already open: press its first row so the keys that follow reach it.
        where = hand.point(*manager, view["rows"][0]["row"], 0, view["picture"])
        hand.inject([moment(ctx, "PointerButton", button=1, pressed=p, x=where["x"],
                            y=where["y"], space=where["space"]) for p in (True, False)])
    seen = []
    for key in ("down", "up"):
        previous = None
        for _ in range(64):
            view = painted(hand, *manager)
            chosen = [r["text"] for r in view["rows"] if r["text"].startswith(">")] if view else []
            if chosen and chosen[0].rstrip().endswith(" " + name):
                hand.inject(chord_moments(ctx, "enter"))
                return {"chosen": chosen[0]}
            seen += chosen
            if chosen == previous:
                break
            previous = chosen
            hand.inject(chord_moments(ctx, key))
    ctx.produce("failed-step-rows.json", json.dumps(sorted(set(seen)), indent=1).encode())
    ctx.fail("the Pane Manager lists no pane named %r" % name)
