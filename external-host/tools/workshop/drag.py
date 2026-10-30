# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Press, request Zengine-owned timed motion, release; retain before, held and after evidence, and
say what the drag's receiver said.

THE RECEIVER'S WORDS are what Workshop says (`NoticeSaid` v1) that the press or the release
caused, heard as `workshop/act` hears its presses: a refusal among them fails the run in
Workshop's words, and a drag nothing answered is said so and fails. Where the row may not hear
them, the summary says that whether anything answered cannot be told here, and never that the drag
was done. The pictures are kept as Workshop's medium gives them: `<name>.bmp` in a window,
`<name>.cells.txt` in a terminal."""
from act import Heard, HeardHand
from hand import Hand
from workshop_steps import MODIFIERS, button_of, cells_point, link_session, picture


def kept(ctx, link, name, names):
    """A picture, kept under the name its format gives it, that name added to `names`."""
    captured, data = picture(ctx, link, name)
    names.append("%s.%s" % (name, "bmp" if captured.get("format") == "image/bmp" else "cells.txt"))
    return captured, data


def run(ctx):
    link = ctx.inputs["link"]
    start = cells_point(ctx, link, ctx.inputs["start"])
    end = cells_point(ctx, link, ctx.inputs["end"])
    if start[2] != end[2]:
        raise ValueError("start and end must use the same coordinate space")
    link_session(ctx, link)
    heard = Heard(ctx, link)
    hand = HeardHand(ctx, link, heard) if heard.sub is not None else Hand(ctx, link)
    heard.step = {"index": 0, "verb": "drag", "args": [ctx.inputs["start"], ctx.inputs["end"]]}
    names = []
    try:
        before, _ = kept(ctx, link, "before", names)
        rows = [dict(x=p[0], y=p[1], space=p[2]) for p in (start, end)]
        modifiers = 0
        for word in filter(None, ctx.inputs.get("hold", "").lower().split("+")):
            if word not in MODIFIERS:
                raise ValueError("'%s' is not a modifier (shift, ctrl, alt)" % word)
            modifiers |= MODIFIERS[word]
        moved = hand.drag(*rows, ctx.inputs.get("duration_ms", 900), ctx.inputs.get("bend", 0),
                          button=button_of(ctx.inputs.get("button", "left")),
                          held=lambda: kept(ctx, link, "held", names), modifiers=modifiers)
        after, _ = kept(ctx, link, "after", names)
        ctx.check(after["frame"] > before["frame"], "no later frame after the drag")
        hand.close()
    finally:
        heard.close()
    said = [s["text"] for s in heard.step.get("caused", [])]
    head = "timed drag published %d movement samples; frame %d -> %d; pictures %s" % (
        moved["admitted"], before["frame"], after["frame"], ", ".join(names))
    if heard.sub is None and heard.why:
        return "%s; whether anything answered it cannot be told here: %s" % (head, heard.why)
    ctx.check(said, "%s; nothing answered the drag: it was pressed, moved and released, and "
                    "Workshop said nothing its press or release caused" % head)
    return "%s; Workshop said: %s" % (head, " | ".join(said))
