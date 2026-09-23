# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""One primary drag, sent in one batch, with actual before/after captures.

Settlement proves dispatch, not that a receiver accepted the value. Read the picture or ask
the destination owner for the resulting state when verifying a particular application story.
"""
from loom_session.tool import LinkOutcome
from workshop_steps import link_session, moment, picture, point


def run(ctx):
    start = point(ctx.inputs["start"])
    end = point(ctx.inputs["end"])
    if start[2] != end[2]:
        raise ValueError("start and end must use the same coordinate space")
    link = ctx.inputs["link"]
    link_session(ctx, link)
    opened = ctx.ask("zengine.input", "InputSessionRequested", {"purpose": "pane value drag"}, via=link)
    session = opened["session"]
    ctx.check(session > 0, "input returned no session")
    state = {"open": True}

    def close():
        if not state["open"]:
            return
        try:
            ctx.ask("zengine.input", "InputSessionClosed", {"session": session, "holder": 0}, via=link)
            state["open"] = False
        except LinkOutcome as out:
            if out.state in ("lost", "unlinked"):
                raise RuntimeError("link unavailable: Workshop owns closing input session %d" % session) from out
            raise

    ctx.on_cleanup(close, "close drag input session %d" % session)
    ctx.step("picture before")
    before, _ = picture(ctx, link, "before")
    x, y, space = start
    tx, ty, _ = end
    events = [moment(ctx, "PointerButton", button=1, pressed=True, x=x, y=y, space=space),
              moment(ctx, "PointerMoved", x=tx, y=ty, dx=tx-x, dy=ty-y, space=space),
              moment(ctx, "PointerButton", button=1, pressed=False, x=tx, y=ty, space=space)]
    ctx.step("drag and settle")
    done = ctx.ask("zengine.input", "InjectInput", {"session": session, "events": events}, via=link, settle=True)
    ctx.check(done["session"] == session and done["admitted"] == 3 and
              done["last_seq"] - done["first_seq"] + 1 == 3, "input did not admit this complete drag")
    ctx.step("picture after")
    after, _ = picture(ctx, link, "after")
    ctx.check(after["frame"] > before["frame"], "no later frame after the drag")
    close()
    return "three drag moments settled; frame %d -> %d; receiver outcome is shown in after.bmp" % (
        before["frame"], after["frame"])
