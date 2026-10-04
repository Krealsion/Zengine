# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Ask Workshop to quit through its one orderly quit, and say what refused it when it stays open.
The launcher separately observes the connection ending."""
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
        # A PRESENTED INTERACTION -- a menu a failed run left open -- is answered first by Escape,
        # as a weaver would.
        ctx.ask("zengine.input", "InjectInput", {"session": opened["session"],
                "events": chord_moments(ctx, "escape")}, via=link, settle=True)
        try:
            ctx.ask("zengine.workshop", "WorkshopQuitRequested", {}, via=link, timeout=60)
        except LinkOutcome:
            raise
        except Exception as refused:
            ctx.check(False, "Workshop stays open: %s" % refused)
    except LinkOutcome as result:
        if result.state not in ("lost", "unlinked"):
            raise
    return "Quit accepted; observe the link closing before declaring the demo stopped."
