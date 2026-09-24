# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Request ordinary orderly quit. The launcher separately observes the connection ending."""
from loom_session.tool import LinkOutcome
from workshop_steps import chord_moments, moment


def run(ctx):
    link = ctx.inputs["link"]
    opened = ctx.ask("zengine.input", "InputSessionRequested", {"purpose": "end owned demo"}, via=link)
    def release():
        try:
            ctx.ask("zengine.input", "InputSessionClosed", {"session": opened["session"], "holder": 0}, via=link)
        except LinkOutcome as result:
            if result.state not in ("lost", "unlinked"):
                raise
    ctx.on_cleanup(release, "release input if the demo is still connected")
    try:
        # A PRESENTED INTERACTION -- a menu a failed run left open -- covers the desk, and Workshop
        # refuses the controls' view under it: Escape answers it first, as a maker would.
        ctx.ask("zengine.input", "InjectInput", {"session": opened["session"],
                "events": chord_moments(ctx, "escape")}, via=link, settle=True)
        view = ctx.ask("zengine.workshop", "PaneViewRequested", {"provider": "zengine.demo", "pane": "controls"}, via=link)
        row = view["rows"][0]
        events = [moment(ctx, "PointerButton", button=1, pressed=p,
                         x=row["x"], y=row["y"], space=row["space"]) for p in (True, False)]
        ctx.ask("zengine.input", "InjectInput", {"session": opened["session"],
                "events": events + chord_moments(ctx, "escape")}, via=link, settle=True)
        ctx.ask("zengine.input", "InjectInput", {"session": opened["session"],
                "events": chord_moments(ctx, "q")}, via=link)
    except LinkOutcome as result:
        if result.state not in ("lost", "unlinked"):
            raise
    return "Quit requested; observe the link closing before declaring the demo stopped."
