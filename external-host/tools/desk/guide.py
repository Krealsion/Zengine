# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""desk/guide -- mark one control of a running Workshop's desk for the weaver to see, under the
guest's own row and its `guide` power, and change nothing else.

It asks Workshop `GuideRequested` v1 by the address a press names: the pane, `<office>/<pane>`, and
the control's action id, which the pane draws as the part `control:<id>`, or `part:<name>` for a
subject part (a Pane Manager row is `part:pane:<office>/<pane>`). An empty `control` clears this
session's mark. The result is Workshop's own answer -- shown, where, and whether its label stands --
or the refusal in words. It presses nothing.
"""

import desk_asks

WORKSHOP = desk_asks.WORKSHOP


def run(ctx):
    link = ctx.inputs.get("link") or "workshop"
    fields = {
        "pane": (ctx.inputs.get("pane") or "").strip(),
        "control": (ctx.inputs.get("control") or "").strip(),
        "words": ctx.inputs.get("words") or "",
    }
    ctx.step("guide")
    asker = desk_asks.Asker(ctx, link)
    answer, refused = asker.ask(WORKSHOP, "GuideRequested", 1, fields)
    if refused is not None:
        return ("desk/guide: %s %s not marked -- %s" % (
            fields["pane"], fields["control"] or "(clear)", refused))[:1900]
    shown = bool(answer.get("shown"))
    return ("desk/guide: %s -- %s" % ("shown" if shown else "not shown",
                                      answer.get("said", "")))[:1900]
