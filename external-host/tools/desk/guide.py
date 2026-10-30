# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""desk/guide -- mark one control of a running Workshop's desk for the weaver to see, under the
guest's own row and its `guide` power, and change nothing else.

It asks Workshop `GuideRequested` v1 by the address a press names: the pane, `<office>/<pane>`, and
the control's action id, which the pane draws as the part `control:<id>`, or `part:<name>` for a
subject part (a Pane Manager row is `part:pane:<office>/<pane>`). An empty `control` clears this
session's mark. The result is Workshop's own answer -- shown, where, and whether its label stands --
written as `guide.json`. A mark Workshop refuses or does not show fails the run in Workshop's
words, a refusal for want of the row's `guide` power saying so; a clear passes whenever Workshop
answers. It presses nothing.
"""

import json

import desk_asks

WORKSHOP = desk_asks.WORKSHOP


#: The bus's word for a send the row's grant does not hold.
DENIED = "CapabilityDenied"


def written(ctx, fields, answer, refused):
    whole = {"asked": fields, "answer": answer, "refused": refused}
    text = json.dumps(whole, indent=2, sort_keys=True, default=str) + "\n"
    ctx.produce("guide.json", text.encode("utf-8"))


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
    written(ctx, fields, answer, refused)
    if refused is not None:
        power = "this row has no guide power -- " if DENIED in refused else ""
        ctx.fail(("desk/guide: %s %s not marked -- %s%s" % (
            fields["pane"], fields["control"] or "(clear)", power, refused))[:1900])
    shown = bool(answer.get("shown"))
    said = "desk/guide: %s -- %s; the answer: guide.json" % ("shown" if shown else "not shown",
                                                             answer.get("said", ""))
    if fields["control"] and not shown:
        ctx.fail(said[:1900])
    return said[:1900]
