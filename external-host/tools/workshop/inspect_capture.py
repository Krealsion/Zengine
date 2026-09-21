# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inspect-capture -- inspect a running Workshop, picture it, inject one batch (a click,
a chord, or both together in that order), picture it again (loom-tool.json describes each input;
this file is the order and the checks).

WHAT MOVES THE JOURNEY: only each owner's own answer to this run's own ask, through the link --
the Input owner's session and injection answers, the guest door's inventory, the Skin's pictures.
Nothing is inferred from a picture or from having been admitted.

`chord` IS OPTIONAL, AND OFTEN SHOULD BE LEFT EMPTY WITH A CLICK. A click alone is a whole,
valid batch, and `click_moments`'s own doc (workshop_steps.py) says why a click that may open a
menu is frequently better off unfollowed by any chord: Workshop counts every key press as a new
gesture, and a menu opened by a now-superseded gesture is refused rather than granted.

WHAT ORDERS THE PICTURE AFTER THE BATCH: the injection is asked with `settle`, so its answer
comes back only once Workshop has dispatched everything the injection set in motion on its bus --
the desktop's handling of it and the repaint among it -- and the "after" picture is asked for
only then. That is causal ordering for that work, not a delay; it says nothing about timers,
other hosts or work deferred to a later turn.

AN ARMED PICTURE is different evidence: a capture asked for BEFORE the batch, for the next
painted frame, which Workshop's Skin holds until a paint passes it. On an idle Workshop that
capture is genuinely pending at its owner until the batch's own repaint answers it. It proves a
far operation was waiting; it is not the "after" picture, because a paint unrelated to the batch
could answer it too.

WHAT IT CLEANS UP: once an input session is open, every way this run can end -- a failed check,
a refusal, a bug, a cancellation -- closes it first and records what the close came to. When the
LINK is gone nothing can be closed from here, and the run says Workshop's guest door closes a
lost guest's session, never that it closed anything.
"""

from loom_session.tool import LinkOutcome, Refused

from workshop_steps import (chord, chord_moments, clear_moments, click_moments, link_session,
                            moment, own_row, picture, point)


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
    click = ctx.inputs.get("click", "")
    chord_spelling = ctx.inputs.get("chord", "")
    button = ctx.inputs.get("button", "left")
    clearing = bool(ctx.inputs.get("clear"))
    text = ctx.inputs.get("text", "")
    repeat = int(ctx.inputs.get("repeat", 1))

    # SPELLING ERRORS END THE RUN BEFORE ANY WORK, same as before: a chord or a point this tool
    # cannot spell is refused here, whether or not it will end up in this batch.
    if click:
        point(click)
    if chord_spelling:
        chord(chord_spelling)

    # A CLICK ALONE IS A WHOLE, VALID BATCH -- `chord` is optional precisely so a click that may
    # open a menu can settle on its own terms, unfollowed by any later gesture that would make
    # it late (`click_moments`'s own doc has the mechanism: `workshop/weave_handlers.cpp`'s
    # `gestures_` counts every key, and a menu opened by a now-superseded gesture is refused
    # "late" by `workshop/weave_external.cpp`, never silently ignored). What is NOT valid is
    # nothing at all, or a click-only batch also asked to clear, type or repeat a chord that
    # is not there to carry them.
    ctx.check(click or chord_spelling, "neither `click` nor `chord` was given -- nothing to inject")
    if not chord_spelling:
        ctx.check(not clearing, "`clear` needs a `chord` to commit what it clears -- name one, "
                                "or drop `clear`")
        ctx.check(not text, "`text` is typed after a chord opens or reaches a field -- name a "
                            "`chord`, or drop `text` for a click-only batch")
        ctx.check(repeat == 1, "`repeat` presses a chord -- name one, or drop `repeat`")

    # THE INTENDED BATCH SIZE, COMPUTED BEFORE ANY WORKSHOP CONTACT -- not after opening an
    # input session and taking the "before" picture, and not by building the real moment list
    # first and discovering its length. This mirrors the construction below exactly (click,
    # then EITHER clear's own fixed four moments plus an optional type, OR a repeated chord
    # plus an optional type), so a `repeat` too large for the rest of the batch is refused in
    # this tool's own words, cheaply, before a single ask leaves this process.
    click_count = 2 if click else 0
    if not chord_spelling:
        expected = click_count
    elif clearing:
        expected = click_count + 4 + (1 if text else 0) + 2
    else:
        expected = click_count + repeat * 2 + (1 if text else 0)
    if expected > 64:
        ctx.fail("%d moment(s) (click=%d, %s) would exceed InjectInput's 64-moment ceiling in "
                 "one batch -- lower `repeat`" %
                 (expected, click_count,
                  ("clear=4, text=%d, chord=2" % (1 if text else 0)) if clearing else
                  ("repeat=%d chord press/release pairs, text=%d" %
                   (repeat, 1 if text else 0))))

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
        ctx.hold(ctx.inputs["hold"], "held before the batch")

    if armed is not None and ctx.inputs.get("await_repaint"):
        # Waiting for Workshop to paint BY ITSELF -- for a Workshop that changes on its own. On an
        # idle one this waits for as long as the tool allows; if the link goes meanwhile, the
        # crossing is lost and its outcome is unknown.
        ctx.step("await a repaint")
        armed.wait(float(ctx.inputs.get("await_seconds", 60)))

    if click and chord_spelling:
        step_words = "%s-click %s then %s%s" % (
            button, click, ("clear, type, then " if clearing else ""), chord_spelling)
    elif click:
        step_words = "%s-click %s, alone -- no chord follows it in this batch" % (button, click)
    else:
        step_words = "%s%s" % (("clear, type, then " if clearing else ""), chord_spelling)
    ctx.step("inject %s%s" % (step_words, (" x%d" % repeat) if repeat != 1 else ""))
    # ONE ORDERED BATCH, in the order a hand would make it: a click that points at a pane,
    # POSSIBLY THE WHOLE BATCH (see `click_moments`'s own doc for why a click that may open a
    # menu is often better left unfollowed by any chord in the same batch); otherwise EITHER a
    # typed field's own order (Ctrl+A selecting the field's whole content regardless of its
    # length or the caret's position, then Backspace erasing that selection whether or not any
    # text follows it, the replacement text if there is one, and only then the chord that
    # COMMITS it -- Return on a line, never before what it submits) OR the plain tool's original
    # order (the chord -- pressed `repeat` time(s), a hand tapping the same key again rather
    # than holding it -- and text typed into what it opened -- ctrl+p, then a name typed into
    # the pane it raised). The Input weave publishes an injected batch in the order handed --
    # never several batches whose relative order this tool would have to trust separately
    # (investigated, not assumed: see `docs/workshop/external-host.md`'s own account of what
    # settlement orders and does not).
    click_events = click_moments(ctx, click, button) if click else []
    if not chord_spelling:
        events = click_events
    elif clearing:
        events = click_events + clear_moments(ctx) + \
            ([moment(ctx, "TextEntered", text=text)] if text else []) + \
            chord_moments(ctx, chord_spelling)
    else:
        events = click_events + chord_moments(ctx, chord_spelling, text, repeat)
    # AN INTERNAL CONSISTENCY CHECK, NOT A REFUSAL OPPORTUNITY -- the real refusal already
    # happened above, before any Workshop contact, computed by the same arithmetic this
    # construction follows. If the two ever disagreed, sending the mismatched batch anyway
    # would be the wrong failure to have; this says so plainly instead.
    ctx.check(len(events) == expected,
              "this tool's own batch-size arithmetic (%d) does not match what it built (%d) -- "
              "a bug in this tool, not a refusal for its caller to act on" %
              (expected, len(events)))
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
              "the picture after the batch is frame %d, not later than frame %d"
              % (after["frame"], before["frame"]))
    if ctx.inputs.get("changed", True):
        ctx.check(after_bytes != before_bytes,
                  "what Workshop presents did not change after %s" % step_words)

    ctx.step("close input")
    close()
    return ("far session %d ('%s'); %d connection(s); input session %d; %d moment(s) settled; "
            "frame %d -> %d (%s, %dx%d)%s" % (
                status["session"], row["established"], len(inv["rows"]), session,
                done["admitted"], before["frame"], after["frame"], after["format"],
                after["width"], after["height"],
                "; the armed capture was answered at frame %d" % armed_frame
                if armed_frame is not None else ""))
