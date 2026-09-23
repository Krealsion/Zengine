# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Press, request Zengine-owned timed motion, release; retain before/after evidence."""
from hand import Hand
from workshop_steps import link_session, picture, point


def run(ctx):
    start, end = point(ctx.inputs["start"]), point(ctx.inputs["end"])
    if start[2] != end[2]:
        raise ValueError("start and end must use the same coordinate space")
    link = ctx.inputs["link"]
    link_session(ctx, link)
    hand = Hand(ctx, link)
    before, _ = picture(ctx, link, "before")
    rows = [dict(x=p[0], y=p[1], space=p[2]) for p in (start, end)]
    moved = hand.drag(*rows, ctx.inputs.get("duration_ms", 900), ctx.inputs.get("bend", 0))
    after, _ = picture(ctx, link, "after")
    ctx.check(after["frame"] > before["frame"], "no later frame after the drag")
    hand.close()
    return "timed drag published %d movement samples; frame %d -> %d; receiver outcome is shown in after.bmp" % (
        moved["admitted"], before["frame"], after["frame"])
