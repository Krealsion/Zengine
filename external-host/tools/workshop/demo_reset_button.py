# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
from hand import Hand
from demo_setup import ROLE
import json


def run(ctx):
    link = ctx.inputs["link"]
    before = ctx.ask(ROLE, "DemoStatusRequested", {}, via=link)
    hand = Hand(ctx, link)
    try:
        hand.click(hand.row(ROLE, "controls", "Reset demo"))
    finally:
        hand.close()
    result = ctx.ask(ROLE, "DemoReadyRequested", {"generation": before["generation"] + 1}, via=link)
    ctx.check(result["state"] == "ready", result["note"])
    ctx.produce("reset.json", json.dumps(result.fields, indent=2).encode())
    return "Clicked the visible Reset button; its next generation reached ready."
