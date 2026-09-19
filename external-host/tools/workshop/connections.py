# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/connections -- who Workshop says is connected, and which row is this session."""

from workshop_steps import link_session, own_row


def run(ctx):
    link = ctx.inputs["link"]
    ctx.step("link status")
    status = link_session(ctx, link)
    ctx.step("inventory")
    inv, row = own_row(ctx, link, status)
    return "Workshop lists %d connection(s); this session is far session %d, admitted as '%s' " \
           "(claimed '%s')" % (len(inv["rows"]), row["session"], row["established"],
                               row["claimed"])
