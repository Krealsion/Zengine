# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/connections -- who Workshop says is connected, and which row is this session; with
`wait_plan`, first where Workshop's load plan came to rest."""

import json

from workshop_steps import link_session, own_row, plan_at_rest


def run(ctx):
    link = ctx.inputs["link"]
    ctx.step("link status")
    status = link_session(ctx, link)
    rested = ""
    if ctx.inputs.get("wait_plan", False):
        ctx.step("plan at rest")
        plan = plan_at_rest(ctx, link)
        ctx.produce("plan.json", json.dumps(plan.fields, indent=1).encode("utf-8"))
        named = ""
        if plan["waiting_on"]:
            named += ", waiting on '%s'" % plan["waiting_on"]
        if plan["unavailable"]:
            named += ", stepped over %s" % ", ".join("'%s'" % s for s in plan["unavailable"])
        rested = "Workshop's plan is %s%s; " % (plan["state"], named)
    ctx.step("inventory")
    inv, row = own_row(ctx, link, status)
    return rested + "Workshop lists %d connection(s); this session is far session %d, admitted " \
        "as '%s' (claimed '%s')" % (len(inv["rows"]), row["session"], row["established"],
                                    row["claimed"])
