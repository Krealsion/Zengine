# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/act -- act on Workshop through a short list of named steps, checking each by what
Workshop says it holds instead of by comparing pictures. loom-tool.json lists the verbs and their
arguments.

A step is one weaver gesture (press, type, open, select, into, click, part, at, rest, wheel,
control, menu) or one observation (desk, expect, absent, rows, picture, wait). Steps run in order
through ONE input session; the first that cannot be done ends the run naming its index and verb,
and nothing after it is sent. Every step is spelled before any Workshop contact, so a misspelled
list costs nothing.

WHAT A WORD PROVES. A pane's words (PaneView version 3) are Workshop's own account of what a pane
painted -- a text pane's rows, a canvas pane's labels and runs -- each with the place the medium
draws it, located by the same measurer that places a press (hand.py). `expect` therefore proves
presentation: that a pane painted a word, not that the owner it paints about finished its work.
Pair it with that owner's evidence when completion matters. A pane Workshop will not describe
(closed, unsettled, covered) is treated as painting nothing while an `expect` waits, and as a
failure anywhere else. A step that presses a pane's word, part or cell -- part, into, click, at,
wheel, control -- waits while its picture takes no press, as the one a managed opening shows takes
none until its pane draws its own and Workshop says its words and parts with no point
(`hand.takes_no_press`): it reads the pane again, for at most its seconds. The desk (DeskView) is
Workshop's own numbers for every pane on it: `desk` checks a place, a size, a state or the keys by
number, never by a picture.

WHAT A NAME IS. Beside its words a pane names the parts a weaver acts on -- a row, a control, an
element -- with names of its own that it keeps across its redraws, and a menu names its lines by
the rows' ids. `part` presses a part by its pane and its name, wherever the pane last drew it;
`open` presses the Pane Manager's row named for a pane (`pane:<office>/<pane>`); `wheel` turns the
wheel over a part by its name; `select` and `menu` take a name before the text a row or line shows.
A name is the pane's: Workshop carries it as said.

WHAT IT CLEANS UP. hand.py registers the input session's close with the run's cleanup, so a
failed step, a bug or a cancellation still gives Workshop's input session back."""
import json
import math
import re
import time
from pathlib import Path

from loom_session.tool import Refused

from hand import FALLBACK_PACE, NO_PRESS, Hand, on_row, rows_of, takes_no_press
from workshop_steps import chord_moments, moment, picture, png_of, point

VERBS = ("press", "type", "open", "select", "into", "click", "part", "at", "rest", "wheel",
         "control", "menu", "desk", "expect", "absent", "rows", "picture", "wait")
GESTURES = ("press", "type", "open", "select", "into", "click", "part", "at", "rest", "wheel",
            "control", "menu")
MANAGER = ("zengine.desktop", "launcher")
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
        if named == ["open"]:
            pane_ref(step["open"])  # ...and a pane named by anything but its row's name
        if named == ["wheel"]:
            wheel_of(step)  # ...and a wheel that names no pane or turns no notch
    pace = max(0, int(ctx.inputs.get("pace_ms", 0))) / 1000.0
    hand = Hand(ctx, ctx.inputs["link"])
    done = []
    try:
        for i, step in enumerate(steps):
            verb = [v for v in VERBS if v in step][0]
            ctx.step("step %d of %d: %s" % (i + 1, len(steps), verb))
            started = time.monotonic()
            record = {"index": i, "verb": verb, "args": step[verb]}
            hand.unsettled = False  # a step's waits judge its own reads, never a last step's
            record.update(act(ctx, hand, verb, step) or {})
            record["elapsed_ms"] = round((time.monotonic() - started) * 1000, 1)
            done.append(record)
            if pace and verb in GESTURES:
                time.sleep(pace)
    finally:
        ctx.produce("steps.json", json.dumps(done, indent=1).encode())
    said = "%d step(s) done: %s" % (len(done), " ".join(r["verb"] for r in done))
    if hand.waits_said():
        said += "; its waits read again every %g s, not at Workshop's notice: %s" % (
            FALLBACK_PACE, hand.waits_said())
    return said


def spelled(value, verb, count):
    ok = isinstance(value, list) and len(value) >= count and all(isinstance(v, str) for v in value)
    if not ok:
        raise ValueError("%s names [provider, pane%s]" % (verb, ", text" if count > 2 else ", text?"))
    return value


def words_now(hand, provider, pane):
    """The pane's words now, or None while Workshop refuses to describe it."""
    try:
        return hand.words(provider, pane)
    except Refused:
        return None


def pause(hand, end, pane=None):
    """Until what a wait waits on may have moved, or `end`: a picture not yet aimed at is read again
    a tenth of a second later, Workshop's own two hops, which no notice marks; anything else, at the
    desk's next move for `pane` (office, pane), or for the desk (`hand.wait`)."""
    left = end - time.monotonic()
    if left <= 0:
        return
    if hand.unsettled:
        time.sleep(min(0.1, left))
    else:
        hand.wait(left, pane)


def texts(view):
    return [w["text"] for w in view["words"]] if view else []


def names(view):
    """The names a pane's parts carry, as the pane said them."""
    return [p["name"] for p in view.get("parts", [])] if view else []


def part_of(view, name):
    """The part `name` names in a pane's view, or None while the pane draws none by that name."""
    parts = [p for p in view.get("parts", []) if p["name"] == name] if view else []
    return parts[0] if parts else None


def unreached(part):
    """Whether no press reaches a part on its own: Workshop gives it no point -- a point in no
    space, 0 -- where the parts over it take every place of it."""
    return part.get("space", 0) == 0


def lines(view):
    """A pane's words as the lines they stand on, top to bottom: the words whose places overlap
    from top to bottom, left to right, each line `{text, words}` -- its words' characters, joined
    where one ends at the next and with a blank where a gap parts them. A text pane's lines are
    its rows, one word each; a canvas pane drawing a row as several labels or runs reads as one."""
    out = []
    words = view["words"] if view else []
    for w in sorted(words, key=lambda w: (w["place"]["y"], w["place"]["x"])):
        line = out[-1] if out else None
        if line is None or not any(on_row(w, other) for other in line["words"]):
            out.append({"text": w["text"], "words": [w]})
            continue
        last = line["words"][-1]["place"]
        gap = w["place"]["x"] > last["x"] + last["w"]
        line["text"] += (" " if gap else "") + w["text"]
        line["words"].append(w)
    return out


def line_of(view, part):
    """The number, among `lines(view)`, of the line a part stands on, or None."""
    found = [i for i, l in enumerate(lines(view)) if any(on_row(w, part) for w in l["words"])]
    return found[0] if found else None


def rows_by_place(view):
    """A pane's lines as the rows they stand on, counted from the top line's row: each row's text,
    and "" for a row between them that no word stands on. A line's row is its first word's place
    against the top line's first word, in that word's height. A text pane's rows are its lines,
    blank ones among them; a canvas pane draws a blank row as no word, and the row keeps its
    number here all the same."""
    found = lines(view)
    top = found[0]["words"][0]["place"] if found else None
    rows = []
    for line in found:
        place = line["words"][0]["place"]
        at = int(round((place["y"] - top["y"]) / float(top["h"]))) if top["h"] else len(rows)
        rows += [""] * (max(at, len(rows)) - len(rows))
        rows.append(line["text"])
    return rows


def wait_part(hand, provider, pane, name, seconds):
    """Read the pane until it draws a part named `name` in a picture that takes a press."""
    end = time.monotonic() + seconds
    while True:
        view = words_now(hand, provider, pane)
        part = part_of(view, name)
        if (part is not None and not takes_no_press(view)) or time.monotonic() >= end:
            return view, part
        pause(hand, end, (provider, pane))


def no_press_yet(ctx, view, verb, provider, pane, seconds):
    """Fail a step whose pane's picture still takes no press: what it would press reaches nothing."""
    ctx.check(not takes_no_press(view), "%s: %s/%s's picture took no press within %gs: %s"
              % (verb, provider, pane, seconds, NO_PRESS))


def press_view(ctx, hand, verb, provider, pane, seconds):
    """The pane's words once its picture takes a press (`hand.pressing`), for a step that presses:
    a pane Workshop does not describe, or one still taking no press after `seconds`, fails it."""
    try:
        return hand.pressing(provider, pane, seconds)
    except Refused:
        ctx.fail("%s: Workshop does not describe %s/%s now" % (verb, provider, pane))
    except ValueError as taken:
        ctx.fail("%s: %s" % (verb, taken))


def fields_of(answer):
    """An answer's fields, as JSON can keep them."""
    return getattr(answer, "fields", answer)


def wait_words(hand, provider, pane, text, seconds, present=True, pressing=False):
    """Read the pane until a word holds `text` (or, `present` false, until none does) -- and,
    `pressing`, in a picture that takes a press."""
    end = time.monotonic() + seconds
    while True:
        view = words_now(hand, provider, pane)
        words = [w for w in view["words"] if text in w["text"]] if view else []
        if bool(words) == present and not (pressing and takes_no_press(view)):
            return view, words
        if time.monotonic() >= end:
            return view, (words if pressing else None)
        pause(hand, end, (provider, pane))


def painted(hand, provider, pane):
    """A pane's rows now, as its words say them (PaneView version 3, `hand.rows_of`): `{picture,
    canvas, rows: [{row, text, x, y, space}]}`, or None while Workshop refuses to describe it. A
    text pane's `row` is its row, which `hand.point` takes with the picture; a canvas pane's is its
    word's number, and `rows_by_place` numbers its lines by the rows they stand on."""
    try:
        return rows_of(hand.words(provider, pane))
    except Refused:
        return None


def wait_rows(hand, provider, pane, text, seconds, present=True):
    """Read the pane until a row holds `text` (or, `present` false, until none does). A pane
    Workshop will not describe holds nothing either way: the wait goes on, and ends `(None,
    None)`."""
    end = time.monotonic() + seconds
    while True:
        view = painted(hand, provider, pane)
        rows = [r for r in view["rows"] if text in r["text"]] if view else []
        if view is not None and bool(rows) == present:
            return view, rows
        if time.monotonic() >= end:
            return view, None
        pause(hand, end, (provider, pane))


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
        provider, pane, name = spelled(arg, verb, 3)[:3]
        return select_row(ctx, hand, provider, pane, name)
    if verb in ("into", "click"):
        # INTO gives a pane the keys by pressing one of its words (the first, or the one holding
        # the text); CLICK presses where the text itself is painted, in a text pane or a canvas
        # pane alike. Both press only what Workshop says is painted now, located by Workshop: a
        # character by its point, and a blank row, which has no character to name, at the point
        # its word gives.
        provider, pane = spelled(arg, verb, 2)[:2]
        text = arg[2] if len(arg) > 2 else ""
        ctx.check(verb == "into" or text, "click names the text to press on")
        seconds = float(step.get("seconds", 10))
        view, words = wait_words(hand, provider, pane, text, seconds, pressing=True)
        ctx.check(words, "%s: %s/%s paints no word holding %r" % (verb, provider, pane, text))
        no_press_yet(ctx, view, verb, provider, pane, seconds)
        at = words[0]
        where = at if not at["text"] else hand.word_point(
            provider, pane, at["word"], max(0, at["text"].find(text)), view["picture"])
        press_at(ctx, hand, where, step.get("button", "left"))
        return {"word": at["text"], "x": where["x"], "y": where["y"]}
    if verb == "part":
        # A PART BY ITS PANE AND ITS NAME, pressed at the point Workshop gives it now: wherever the
        # pane's last redraw put it, in a text pane or a canvas pane alike.
        provider, pane, name = spelled(arg, verb, 3)[:3]
        seconds = float(step.get("seconds", 10))
        view, part = wait_part(hand, provider, pane, name, seconds)
        if part is None:
            ctx.produce("failed-step-parts.json", json.dumps(
                names(view) if view else "not described", indent=1).encode())
            ctx.fail("part: %s/%s draws no part named %r within %gs" % (provider, pane, name, seconds))
        no_press_yet(ctx, view, verb, provider, pane, seconds)
        ctx.check(not unreached(part), "part: no press reaches %r in %s/%s on its own: the parts "
                  "over it take every place of it" % (name, provider, pane))
        press_at(ctx, hand, part, step.get("button", "left"))
        return {"part": name, "text": part["text"], "x": part["x"], "y": part["y"]}
    if verb == "rest":
        # THE POINTER RESTS where it is put, with no button held -- a window pixel "x,y" or a
        # terminal cell that names its cells. A canvas pane that asks for its hover is told where.
        x, y, space = point(arg)
        hand.inject([moment(ctx, "PointerMoved", x=x, y=y, space=space)])
        return {"x": x, "y": y}
    if verb == "wheel":
        return turn_wheel(ctx, hand, step)
    if verb == "at":
        # ONE CELL OF A PANE'S TEXT LATTICE by its row and column, counted from 0 (`hand.point`): a
        # text pane's painted cell, or the cell of the lattice a canvas pane sets its text on -- a
        # blank row's, or the one after a row's last character, too -- for panes whose meaning is
        # a grid rather than a labelled row. A pane that redrew between the reading and the point
        # is read again.
        ok = isinstance(arg, list) and len(arg) == 4 and all(isinstance(v, str) for v in arg[:2]) \
            and all(isinstance(v, int) for v in arg[2:])
        ctx.check(ok, "at names [provider, pane, row, column]")
        provider, pane, row, column = arg
        for _ in range(3):
            view = press_view(ctx, hand, verb, provider, pane, float(step.get("seconds", 10)))
            try:
                where = hand.point(provider, pane, row, column, view["picture"])
            except Refused as refused:
                if "picture moved" not in str(refused):
                    raise
                continue  # the pane redrew between the reading and the point: read it again
            press_at(ctx, hand, where, step.get("button", "left"))
            return {"x": where["x"], "y": where["y"]}
        ctx.fail("at: %s/%s kept redrawing while it was read" % (provider, pane))
    if verb == "control":
        provider, pane, label = spelled(arg, verb, 3)[:3]
        return press_control(ctx, hand, provider, pane, label, float(step.get("seconds", 10)))
    if verb == "menu":
        return choose(ctx, hand, arg, float(step.get("seconds", 10)), step.get("button", "left"))
    if verb == "desk":
        return check_desk(ctx, hand, arg, step)
    if verb in ("expect", "absent"):
        provider, pane, text = spelled(arg, verb, 3)[:3]
        seconds = float(step.get("seconds", 10))
        view, words = wait_words(hand, provider, pane, text, seconds, verb == "expect")
        if words is None:
            ctx.produce("failed-step-rows.json", json.dumps(
                texts(view) if view else "not described", indent=1).encode())
            ctx.fail("%s: %s/%s %s %r within %gs" % (verb, provider, pane, "never painted"
                     if verb == "expect" else "still paints", text, seconds))
        return {"matched": [w["text"] for w in words]}
    if verb == "rows":
        provider, pane = spelled(arg, verb, 2)[:2]
        view = words_now(hand, provider, pane)
        ctx.check(view is not None, "rows: Workshop does not describe %s/%s now" % (provider, pane))
        kept = step.get("as", pane.replace(".", "-"))
        ctx.produce(kept + ".json", json.dumps({"picture": view["picture"], "rows": texts(view),
                    "words": [dict(w) for w in view["words"]],
                    "parts": [dict(p) for p in view.get("parts", [])]}, indent=1).encode())
        return {"rows": len(view["words"]), "parts": names(view), "picture": view["picture"]}
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


def wheel_of(step):
    """`(provider, pane, name, dx, dy)` of a wheel step -- `{"wheel": [provider, pane, name?],
    "dy": n, "dx": n}`, the notches as the platform reports a wheel's (dy 1 away from the weaver,
    -1 toward) -- or ValueError naming what is wrong, before any contact."""
    arg = step["wheel"]
    if not (isinstance(arg, list) and len(arg) in (2, 3) and all(isinstance(v, str) for v in arg)):
        raise ValueError("wheel names [provider, pane, name?]")
    turned = []
    for axis in ("dx", "dy"):
        value = step.get(axis, 0)
        number = isinstance(value, (int, float)) and not isinstance(value, bool)
        if not number or not math.isfinite(value):
            raise ValueError("wheel's %s is a number of notches, not %r" % (axis, value))
        turned.append(float(value))
    if not any(turned):
        raise ValueError("wheel turns no notch: it names dy (1 away from the weaver, -1 toward) "
                         "or dx")
    return arg[0], arg[1], arg[2] if len(arg) > 2 else "", turned[0], turned[1]


def turn_wheel(ctx, hand, step):
    """ONE WHEEL MOMENT over a pane, where Workshop says a part it names is -- or, naming none, its
    first word -- once the pane draws it: a text pane hears it as its wheel, a canvas pane as a
    canvas wheel. What the pane does with it is the pane's; a step after it says what changed."""
    provider, pane, name, dx, dy = wheel_of(step)
    seconds = float(step.get("seconds", 10))
    if name:
        view, at = wait_part(hand, provider, pane, name, seconds)
        if at is None:
            ctx.produce("failed-step-parts.json", json.dumps(
                names(view) if view else "not described", indent=1).encode())
            ctx.fail("wheel: %s/%s draws no part named %r within %gs"
                     % (provider, pane, name, seconds))
        no_press_yet(ctx, view, "wheel", provider, pane, seconds)
        ctx.check(not unreached(at), "wheel: no point reaches %r in %s/%s on its own: the parts "
                  "over it take every place of it" % (name, provider, pane))
    else:
        view, words = wait_words(hand, provider, pane, "", seconds, pressing=True)
        ctx.check(words, "wheel: %s/%s paints no word within %gs" % (provider, pane, seconds))
        no_press_yet(ctx, view, "wheel", provider, pane, seconds)
        at = words[0]
    hand.inject([moment(ctx, "PointerWheel", wheel_dx=dx, wheel_dy=dy, x=at["x"], y=at["y"],
                        space=at["space"])])
    return {"part": name, "x": at["x"], "y": at["y"], "dx": dx, "dy": dy}


def press_control(ctx, hand, provider, pane, label, seconds):
    """Press a visible `[label]` control, in a text pane's rows or a canvas pane's words: the
    column comes from the painted word, the screen position from Workshop."""
    view = press_view(ctx, hand, "control", provider, pane, seconds)
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
    """Press the line of the menu on the screen named `text` -- a row's id, as the menu names its
    lines -- or, where no line carries that name, the one line that holds `text`, once the desk
    shows a menu with it: one line exactly, or the run fails with the lines it showed."""
    ctx.check(isinstance(text, str) and text, "menu names a line by its name or its text")
    end = time.monotonic() + seconds
    while True:
        menu = hand.desk()["menu"]
        named = [p for p in menu.get("parts", []) if p["name"] == text] if menu["open"] else []
        if named:
            ctx.check(not unreached(named[0]), "menu: no press reaches the line named %r on its "
                      "own: the parts over it take every place of it" % text)
            press_at(ctx, hand, named[0], button)
            return {"line": named[0]["text"], "name": text, "office": menu["office"],
                    "pane": menu["pane"]}
        lines = [l for l in menu["lines"] if text in l["text"]] if menu["open"] else []
        if len(lines) > 1:
            break
        if lines:
            press_at(ctx, hand, lines[0], button)
            return {"line": lines[0]["text"], "office": menu["office"], "pane": menu["pane"]}
        if time.monotonic() >= end:
            break
        pause(hand, end)
    ctx.produce("failed-step-rows.json", json.dumps(
        {"lines": [l["text"] for l in menu["lines"]], "names": [p["name"] for p in menu.get("parts", [])]}
        if menu["open"] else "no menu", indent=1).encode())
    ctx.fail("menu: %s" % ("%d lines hold %r" % (len(lines), text) if lines
                           else "no menu line is named or holds %r within %gs" % (text, seconds)))


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
        pause(hand, end, tuple(arg) if arg else None)
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
    it is the row `name` names, and stop there. A row is named by the part the pane names it
    (`property:Height`), or, in a pane naming no part so, by its text (`row_names`: after a one- or
    two-column marker, the name, and a value a list may set beside it). A named row already painted
    sets the direction; otherwise Down to the list's end, then Up. The walk is bounded: a direction
    ends where the marked row stops moving. A name no row carries, or text more than one row
    carries, fails with the rows the pane painted, and nothing is pressed after that. Nothing is
    chosen by position: whatever else the list holds, and in whatever order, the marked row is read
    back before the step ends."""
    seen, presses = [], 0

    def look():
        view = words_now(hand, provider, pane)
        ctx.check(view is not None, "select: Workshop does not describe %s/%s now" % (provider, pane))
        rows = [l["text"] for l in lines(view)]
        chosen = [i for i, r in enumerate(rows) if r.startswith(">")]
        part = part_of(view, name)
        if part is not None:
            line = line_of(view, part)
            named = [line] if line is not None else []
        else:
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


def manager_holds_keys(hand):
    """Whether the desk says the Pane Manager holds the keys."""
    on = [p for p in hand.desk()["panes"] if (p["provider"], p["pane"]) == MANAGER]
    return bool(on) and on[0]["keys"]


def pane_ref(name):
    """The office and pane a pane's row is named for, as the Pane Manager and Info name it:
    `pane:<office>/<pane>`."""
    ref = name[len("pane:"):] if isinstance(name, str) and name.startswith("pane:") else ""
    office, _, pane = ref.partition("/")
    if not office or not pane:
        raise ValueError("a pane is named by its row's name, pane:<office>/<pane> -- not %r" % (name,))
    return office, pane


def marked(hand, view, part):
    """Whether the Pane Manager marks the row a part stands on and holds the keys: a press there
    opens its pane."""
    line = line_of(view, part)
    return (line is not None and lines(view)[line]["text"].startswith(">")
            and manager_holds_keys(hand))


def open_pane(ctx, hand, name, seconds):
    """Open, or go to, the pane whose Pane Manager row is named `name` -- `pane:<office>/<pane>`,
    the pane's reference: the Pane Manager put on the desk (Ctrl+P) when it is not there, the row
    brought into its window with Down and Up when the window leaves it out, then pressed where
    Workshop says its name is. A press on a row not yet marked chooses it and gives the Pane Manager
    the keys; a press on the marked row with the keys already there opens the pane, or focuses it.
    The step ends when the desk says the pane is selected -- what a launch makes it, opened or not --
    and says the state it is in; it fails with the names the list said when none is `name`."""
    office, pane = pane_ref(name)
    view = words_now(hand, *MANAGER)
    if view is None or not view["words"]:
        # Ctrl+P toggles: it opens a closed Pane Manager and closes an open one -- including one
        # that is open but covered, which Workshop does not describe. So when the first press
        # brings no describable Pane Manager it may have hidden a covered one: press it again.
        hand.inject(chord_moments(ctx, "ctrl+p"))
        view, words = wait_words(hand, *MANAGER, "PANES", 1.5)
        if not words:
            hand.inject(chord_moments(ctx, "ctrl+p"))
            view, words = wait_words(hand, *MANAGER, "PANES", seconds)
        ctx.check(words, "the Pane Manager did not open")
    seen = set(names(view))
    if part_of(view, name) is None:
        # THE ROW IS OUT OF THE LIST'S WINDOW: the keys to the Pane Manager by a press on its top
        # line's first word, then Down to the list's end and Up, until the window draws it. Bounded.
        first = lines(view)[0]["words"][0]
        press_at(ctx, hand, first if not first["text"] else
                 hand.word_point(*MANAGER, first["word"], 0, view["picture"]))
        for key in ("down", "up"):
            previous = None
            for _ in range(64):
                view = words_now(hand, *MANAGER)
                seen.update(names(view))
                if part_of(view, name) is not None or texts(view) == previous:
                    break
                previous = texts(view)
                hand.inject(chord_moments(ctx, key))
            if part_of(view, name) is not None:
                break
    part = part_of(view, name)
    if part is None:
        ctx.produce("failed-step-parts.json", json.dumps(sorted(seen), indent=1).encode())
        ctx.fail("open: the Pane Manager names no row %r" % name)
    ctx.check(not unreached(part), "open: no press reaches the Pane Manager's row %r on its own: "
              "the parts over it take every place of it" % name)
    presses = 1
    if not marked(hand, view, part):
        # THE FIRST PRESS CHOOSES THE ROW and brings the keys; the second, once the Pane Manager
        # shows both, opens it -- never a second press read against the picture the first changed.
        press_at(ctx, hand, part)
        presses += 1
        end = time.monotonic() + seconds
        while True:
            view = words_now(hand, *MANAGER)
            part = part_of(view, name)
            ctx.check(part is not None, "open: the Pane Manager stopped naming %r" % name)
            ctx.check(not unreached(part), "open: no press reaches the Pane Manager's row %r on "
                      "its own: the parts over it take every place of it" % name)
            if marked(hand, view, part):
                break
            if time.monotonic() >= end:
                ctx.fail("open: the Pane Manager did not mark %r within %gs" % (name, seconds))
            pause(hand, end, MANAGER)
    press_at(ctx, hand, part)
    end = time.monotonic() + seconds
    while True:
        desk = hand.desk()
        on = [p for p in desk["panes"] if p["provider"] == office and p["pane"] == pane]
        if on and on[0]["selected"] and on[0]["state"] != "closed":
            return {"opened": name, "row": part["text"], "presses": presses, "state": on[0]["state"]}
        if time.monotonic() >= end:
            ctx.produce("failed-step-desk.json", json.dumps(fields_of(desk), indent=1).encode())
            ctx.fail("open: %s is %s within %gs" % (name[len("pane:"):], "%s and %s" % (
                on[0]["state"], "selected" if on[0]["selected"] else "not selected")
                if on else "not on the desk", seconds))
        pause(hand, end)
