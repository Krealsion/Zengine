# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/act -- act on Workshop through a short list of named steps, checking each by what
Workshop says it holds instead of by comparing pictures. loom-tool.json lists the verbs and their
arguments.

A step is one weaver gesture (press, type, open, select, into, click, at, rest, control, menu) or
one observation (desk, expect, absent, rows, picture, wait). Steps run in order through ONE input
session; the first that cannot be done ends the run naming its index and verb, and nothing after it
is sent. Every step is spelled before any Workshop contact, so a misspelled list costs nothing.

WHAT A WORD PROVES. A pane's words (PaneView version 2) are Workshop's own account of what a pane
painted -- a text pane's rows, a canvas pane's labels and runs -- each with the place the medium
draws it, located by the same measurer that places a press (hand.py). `expect` therefore proves
presentation: that a pane painted a word, not that the owner it paints about finished its work.
Pair it with that owner's evidence when completion matters. A pane Workshop will not describe
(closed, unsettled, covered) is treated as painting nothing while an `expect` waits, and as a
failure anywhere else. The desk (DeskView) is Workshop's own numbers for every pane on it: `desk`
checks a place, a size, a state or the keys by number, never by a picture.

WHAT IT CLEANS UP. hand.py registers the input session's close with the run's cleanup, so a
failed step, a bug or a cancellation still gives Workshop's input session back."""
import json
import re
import time
from pathlib import Path

from loom_session.tool import Refused

from hand import Hand
from workshop_steps import chord_moments, moment, picture, png_of, point

VERBS = ("press", "type", "open", "select", "into", "click", "at", "rest", "control", "menu",
         "desk", "expect", "absent", "rows", "picture", "wait")
GESTURES = ("press", "type", "open", "select", "into", "click", "at", "rest", "control", "menu")
BUTTONS = {"left": 1, "right": 3}


def run(ctx):
    steps = json.loads(ctx.inputs["steps"])
    ctx.check(isinstance(steps, list) and steps, "steps must be a non-empty JSON list")
    for i, step in enumerate(steps):
        named = [v for v in VERBS if isinstance(step, dict) and v in step]
        ctx.check(len(named) == 1, "step %d names %s; each step names exactly one of %s"
                  % (i, named or "no verb", ", ".join(VERBS)))
        if named == ["rest"]:
            point(step["rest"])  # a misspelled point is refused before any contact
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
    """The pane's words now, or None while Workshop refuses to describe it."""
    try:
        return hand.words(provider, pane)
    except Refused:
        return None


def texts(view):
    return [w["text"] for w in view["words"]] if view else []


def fields_of(answer):
    """An answer's fields, as JSON can keep them."""
    return getattr(answer, "fields", answer)


def wait_words(hand, provider, pane, text, seconds, present=True):
    """Read the pane until a word holds `text` (or, `present` false, until none does)."""
    end = time.monotonic() + seconds
    while True:
        view = painted(hand, provider, pane)
        words = [w for w in view["words"] if text in w["text"]] if view else []
        if bool(words) == present:
            return view, words
        if time.monotonic() >= end:
            return view, None
        time.sleep(0.2)


def press_at(ctx, hand, where, button="left"):
    hand.inject([moment(ctx, "PointerButton", button=BUTTONS[button], pressed=p, x=where["x"],
                        y=where["y"], space=where["space"]) for p in (True, False)])


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
        # INTO gives a pane the keys by pressing one of its words (the first, or the one holding
        # the text); CLICK presses where the text itself is painted, in a text pane or a canvas
        # pane alike. Both press only what Workshop says is painted now, located by Workshop.
        provider, pane = names(arg, verb, 2)[:2]
        text = arg[2] if len(arg) > 2 else ""
        ctx.check(verb == "into" or text, "click names the text to press on")
        view, words = wait_words(hand, provider, pane, text, float(step.get("seconds", 10)))
        ctx.check(words, "%s: %s/%s paints no word holding %r" % (verb, provider, pane, text))
        at = words[0]
        where = hand.word_point(provider, pane, at["word"], max(0, at["text"].find(text)),
                                view["picture"])
        press_at(ctx, hand, where, step.get("button", "left"))
        return {"word": at["text"], "x": where["x"], "y": where["y"]}
    if verb == "rest":
        # THE POINTER RESTS where it is put, with no button held -- a window pixel "x,y" or a
        # terminal cell that names its cells. A canvas pane that asks for its hover is told where.
        x, y, space = point(arg)
        hand.inject([moment(ctx, "PointerMoved", x=x, y=y, space=space)])
        return {"x": x, "y": y}
    if verb == "at":
        # One painted cell by its row and column in a text pane's own lattice (its words are its
        # rows, counted from 0): for panes whose meaning is a grid rather than a labelled row.
        ok = isinstance(arg, list) and len(arg) == 4 and all(isinstance(v, str) for v in arg[:2]) \
            and all(isinstance(v, int) for v in arg[2:])
        ctx.check(ok, "at names [provider, pane, row, column]")
        view = painted(hand, arg[0], arg[1])
        ctx.check(view is not None, "at: Workshop does not describe %s/%s now" % (arg[0], arg[1]))
        where = hand.point(arg[0], arg[1], arg[2], arg[3], view["picture"])
        press_at(ctx, hand, where, step.get("button", "left"))
        return {"x": where["x"], "y": where["y"]}
    if verb == "control":
        provider, pane, label = names(arg, verb, 3)[:3]
        return press_control(ctx, hand, provider, pane, label)
    if verb == "menu":
        return choose(ctx, hand, arg, float(step.get("seconds", 10)), step.get("button", "left"))
    if verb == "desk":
        return check_desk(ctx, hand, arg, step)
    if verb in ("expect", "absent"):
        provider, pane, text = names(arg, verb, 3)[:3]
        seconds = float(step.get("seconds", 10))
        view, words = wait_words(hand, provider, pane, text, seconds, verb == "expect")
        if words is None:
            ctx.produce("failed-step-rows.json", json.dumps(
                texts(view) if view else "not described", indent=1).encode())
            ctx.fail("%s: %s/%s %s %r within %gs" % (verb, provider, pane, "never painted"
                     if verb == "expect" else "still paints", text, seconds))
        return {"matched": [w["text"] for w in words]}
    if verb == "rows":
        provider, pane = names(arg, verb, 2)[:2]
        view = painted(hand, provider, pane)
        ctx.check(view is not None, "rows: Workshop does not describe %s/%s now" % (provider, pane))
        kept = step.get("as", pane.replace(".", "-"))
        ctx.produce(kept + ".json", json.dumps({"picture": view["picture"], "rows": texts(view),
                    "words": [dict(w) for w in view["words"]]}, indent=1).encode())
        return {"rows": len(view["words"]), "picture": view["picture"]}
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


def press_control(ctx, hand, provider, pane, label):
    """Press a visible `[label]` control, in a text pane's rows or a canvas pane's words: the
    column comes from the painted word, the screen position from Workshop."""
    view = painted(hand, provider, pane)
    ctx.check(view is not None, "control: Workshop does not describe %s/%s now" % (provider, pane))
    word = "[" + label + "]"
    for w in view["words"]:
        at = w["text"].find(word)
        if at >= 0:
            where = hand.word_point(provider, pane, w["word"], at + 1, view["picture"])
            press_at(ctx, hand, where)
            return {"word": w["text"], "x": where["x"], "y": where["y"]}
    ctx.produce("failed-step-rows.json", json.dumps(texts(view), indent=1).encode())
    ctx.fail("control: no visible %s in %s/%s" % (word, provider, pane))


def choose(ctx, hand, text, seconds, button):
    """Press the line of the menu on the screen that holds `text`, once the desk shows a menu
    holding it: one line exactly, or the run fails with the lines it showed."""
    ctx.check(isinstance(text, str) and text, "menu names the text of the line to press")
    end = time.monotonic() + seconds
    while True:
        menu = hand.desk()["menu"]
        lines = [l for l in menu["lines"] if text in l["text"]] if menu["open"] else []
        if len(lines) > 1:
            break
        if lines:
            press_at(ctx, hand, lines[0], button)
            return {"line": lines[0]["text"], "office": menu["office"], "pane": menu["pane"]}
        if time.monotonic() >= end:
            break
        time.sleep(0.2)
    ctx.produce("failed-step-rows.json", json.dumps(
        [l["text"] for l in menu["lines"]] if menu["open"] else "no menu", indent=1).encode())
    ctx.fail("menu: %s" % ("%d lines hold %r" % (len(lines), text) if lines
                           else "no menu line holds %r within %gs" % (text, seconds)))


DESK_FIELDS = ("arranging", "menu", "width", "height", "cell_px", "space", "room")
PANE_FIELDS = ("name", "state", "front", "selected", "keys", "visible", "resolved")


def holds(said, wanted):
    """Whether what the desk said holds every number `wanted` names (a nested place in part)."""
    if isinstance(wanted, dict):
        return isinstance(said, dict) and all(k in said and holds(said[k], v)
                                              for k, v in wanted.items())
    return said == wanted


def check_desk(ctx, hand, arg, step):
    """Read the desk until it says what `is` names, of a pane ([provider, pane]) or of the desk
    itself ([]): a place or a size by its number, a state, the rank, the keys. A desk that never
    says so fails with what it last said."""
    ctx.check(isinstance(arg, list) and len(arg) in (0, 2) and all(isinstance(v, str) for v in arg),
              "desk names [provider, pane], or [] for the desk itself")
    wanted = step.get("is", {})
    ctx.check(isinstance(wanted, dict), "desk's is names fields and the numbers they hold")
    allowed = PANE_FIELDS if arg else DESK_FIELDS
    unknown = sorted(set(wanted) - set(allowed))
    ctx.check(not unknown, "desk: %s %s not one of %s" % (", ".join(unknown),
              "is" if len(unknown) == 1 else "are", ", ".join(allowed)))
    seconds = float(step.get("seconds", 10))
    end = time.monotonic() + seconds
    while True:
        desk = hand.desk()
        if arg:
            rows = [p for p in desk["panes"] if p["provider"] == arg[0] and p["pane"] == arg[1]]
            said = rows[0] if rows else None
        else:
            said = dict((k, desk[k]) for k in DESK_FIELDS if k != "menu")
            said["menu"] = desk["menu"]["open"]
        if said is not None and holds(said, wanted):
            if step.get("as"):
                ctx.produce(step["as"] + ".json", json.dumps(fields_of(desk), indent=1).encode())
            return {"said": said}
        if time.monotonic() >= end:
            break
        time.sleep(0.2)
    ctx.produce("failed-step-desk.json", json.dumps(fields_of(desk), indent=1).encode())
    ctx.fail("desk: %s %s within %gs (it said %s)" % (
        "/".join(arg) if arg else "the desk", "is not on the desk" if said is None
        else "never held %s" % json.dumps(wanted), seconds,
        json.dumps(said) if said is not None else "no such pane"))


def row_names(text, name):
    """Whether a list row names `name`: its text after the marker -- one column (`>` or a blank),
    and the blank after it where the marker is two columns wide -- is the name, or the name and a
    gap of two blanks before a value a list sets beside it (Info's property rows:
    `>Height      300`)."""
    body = text[1:]
    if body.startswith(" "):
        body = body[1:]
    body = body.rstrip()
    return body == name or body.startswith(name + "  ")


def select_row(ctx, hand, provider, pane, name):
    """Move a list's cursor -- the row the pane paints with a leading '>' -- with Down and Up until
    it names exactly `name` (`row_names`: after a one- or two-column marker, the name, and a value
    a list may set beside it), and stop there. A row naming it that is already painted sets the direction; otherwise
    Down to the list's end, then Up. The walk is bounded: a direction ends where the marked row
    stops moving. A name no row carries, or more than one row carries, fails with the rows the
    pane painted, and nothing is pressed after that. Nothing is chosen by position: whatever else
    the list holds, and in whatever order, the marked row is read back before the step ends."""
    seen, presses = [], 0

    def look():
        view = painted(hand, provider, pane)
        ctx.check(view is not None, "select: Workshop does not describe %s/%s now" % (provider, pane))
        rows = texts(view)
        chosen = [i for i, r in enumerate(rows) if r.startswith(">")]
        named = [i for i, r in enumerate(rows) if r[:1] in (">", " ") and row_names(r, name)]
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
    if view is None or not view["words"]:
        # Ctrl+P toggles: it opens a closed Pane Manager and closes an open one -- including one
        # that is open but covered, which Workshop does not describe. So when the first press
        # brings no describable Pane Manager it may have hidden a covered one: press it again.
        hand.inject(chord_moments(ctx, "ctrl+p"))
        view, words = wait_words(hand, *manager, "PANES", 1.5)
        if not words:
            hand.inject(chord_moments(ctx, "ctrl+p"))
            view, words = wait_words(hand, *manager, "PANES", seconds)
        ctx.check(words, "the Pane Manager did not open")
    else:
        # Already open: press its first word so the keys that follow reach it.
        where = hand.word_point(*manager, view["words"][0]["word"], 0, view["picture"])
        press_at(ctx, hand, where)
    seen = []
    for key in ("down", "up"):
        previous = None
        for _ in range(64):
            view = painted(hand, *manager)
            chosen = [t for t in texts(view) if t.startswith(">")]
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
