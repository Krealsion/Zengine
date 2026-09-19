# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inspect-capture -- inspect a running Workshop, picture it, press a chord, picture it
again (loom-tool.json describes each input; this file is the order and the checks).

WHAT MOVES THE JOURNEY: only each owner's own answer to this run's own ask, through the link --
the Input owner's session and injection answers, the guest door's inventory, the Skin's pictures.
Nothing is inferred from a picture or from having been admitted.

WHAT ORDERS THE PICTURE AFTER THE CHORD: the injection is asked with `settle`, so its answer comes
back only once Workshop has dispatched everything the injection set in motion on its bus -- the
desktop's handling of the chord and the repaint among it -- and the "after" picture is asked for
only then. That is causal ordering for that work, not a delay; it says nothing about timers,
other hosts or work deferred to a later turn.

AN ARMED PICTURE is different evidence: a capture asked for BEFORE the chord, for the next painted
frame, which Workshop's Skin holds until a paint passes it. On an idle Workshop that capture is
genuinely pending at its owner until the chord's repaint answers it. It proves a far operation was
waiting; it is not the "after" picture, because a paint unrelated to the chord could answer it too.

WHAT IT CLEANS UP: once an input session is open, every way this run can end -- a failed check,
a refusal, a bug, a cancellation -- closes it first and records what the close came to. When the
LINK is gone nothing can be closed from here, and the run says Workshop's guest door closes a
lost guest's session, never that it closed anything.
"""

from loom_session.tool import LinkOutcome, Refused

from workshop_steps import chord, chord_moments, link_session, own_row, picture


def run(ctx):
    # THE LINK'S OWN WORD is a verdict here, never an unexplained error: `lost` is an UNKNOWN
    # outcome -- the request may or may not have acted -- and nothing is resent.
    try:
        return journey(ctx)
    except LinkOutcome as out:
        if out.unknown:
            ctx.fail("the link was lost after %s was submitted (attempt %d): its outcome is "
                     "UNKNOWN, and nothing was resent" % (out.shape, out.attempt))
        ctx.fail("the link says %s at %s: %s" % (out.state, out.shape, out.reason))


def journey(ctx):
    link = ctx.inputs["link"]
    chord(ctx.inputs["chord"])  # a chord this tool cannot spell ends the run before any work
    bug_after = ctx.inputs.get("bug_after", "")

    ctx.step("link status")
    status = link_session(ctx, link)
    ctx.step("inspect")
    inv, row = own_row(ctx, link, status)
    if bug_after == "inspect":
        raise RuntimeError("a deliberate tool bug after 'inspect'")

    ctx.step("open input")
    try:
        opened = ctx.ask("zengine.input", "InputSessionRequested",
                         {"purpose": "loom session run %s" % ctx.name}, via=link)
    except Refused as err:
        # A refusal AFTER work began (the inventory above is kept), in the Input owner's own
        # sentence. One holder at a time -- and every run on one link is the SAME holder to
        # Workshop, which is why its sentence may say "held by you".
        ctx.fail("zengine.input refused an input session: %s (every run on link '%s' is one "
                 "participant to Workshop, far session %d)" % (err.reason, link, status["session"]))
    session = opened["session"]
    ctx.check(session > 0, "zengine.input opened no session it can name (%d)" % session)
    state = {"open": True}

    def close():
        if not state["open"]:
            return
        try:
            ctx.ask("zengine.input", "InputSessionClosed", {"session": session, "holder": 0},
                    via=link, timeout=20.0)
            state["open"] = False
        except LinkOutcome as out:
            if out.state in ("lost", "unlinked"):
                raise RuntimeError("the link is %s: session %d is Workshop's to close -- its guest "
                                   "door closes a lost guest's input session; nothing was closed "
                                   "from here" % (out.state, session))
            raise

    ctx.on_cleanup(close, "close input session %d" % session)
    if bug_after == "open":
        raise RuntimeError("a deliberate tool bug after 'open'")

    ctx.step("picture before")
    before, before_bytes = picture(ctx, link, "before")

    armed = None
    if ctx.inputs.get("arm"):
        ctx.step("arm the next picture")
        armed = ctx.ask_async("zengine.skin", "SurfaceCaptureRequested",
                              {"after_frame": before["frame"]}, via=link)
        ctx.note("capture armed at Workshop's Skin for the first frame after %d" % before["frame"])

    if ctx.inputs.get("hold"):
        ctx.hold(ctx.inputs["hold"], "held before the chord")

    if armed is not None and ctx.inputs.get("await_repaint"):
        # Waiting for Workshop to paint BY ITSELF -- for a Workshop that changes on its own. On an
        # idle one this waits for as long as the tool allows; if the link goes meanwhile, the
        # crossing is lost and its outcome is unknown.
        ctx.step("await a repaint")
        armed.wait(float(ctx.inputs.get("await_seconds", 60)))

    ctx.step("inject %s" % ctx.inputs["chord"])
    events = chord_moments(ctx, ctx.inputs["chord"], ctx.inputs.get("text", ""))
    done = ctx.ask("zengine.input", "InjectInput", {"session": session, "events": events},
                   via=link, settle=True)
    ctx.check(done["session"] == session and done["admitted"] == len(events) and
              done["last_seq"] - done["first_seq"] + 1 == len(events),
              "zengine.input answered for session %d with %d moment(s) (seq %d..%d), not the %d of "
              "session %d that were sent" % (done["session"], done["admitted"], done["first_seq"],
                                             done["last_seq"], len(events), session))
    if bug_after == "inject":
        raise RuntimeError("a deliberate tool bug after 'inject'")

    armed_frame = None
    if armed is not None:
        # Fetched BEFORE the next picture is asked for: the Skin retains one picture at a time.
        ctx.step("collect the armed picture")
        taken, _ = picture(ctx, link, "armed", pending=armed)
        ctx.check(taken["frame"] > before["frame"],
                  "the armed picture is frame %d, not a frame after %d"
                  % (taken["frame"], before["frame"]))
        armed_frame = taken["frame"]

    ctx.step("picture after")
    after, after_bytes = picture(ctx, link, "after")
    ctx.check(after["frame"] > before["frame"],
              "the picture after the chord is frame %d, not later than frame %d"
              % (after["frame"], before["frame"]))
    if ctx.inputs.get("changed", True):
        ctx.check(after_bytes != before_bytes,
                  "what Workshop presents did not change after %s" % ctx.inputs["chord"])

    ctx.step("close input")
    close()
    return ("far session %d ('%s'); %d connection(s); input session %d; %d moment(s) settled; "
            "frame %d -> %d (%s, %dx%d)%s" % (
                status["session"], row["established"], len(inv["rows"]), session,
                done["admitted"], before["frame"], after["frame"], after["format"],
                after["width"], after["height"],
                "; the armed capture was answered at frame %d" % armed_frame
                if armed_frame is not None else ""))
