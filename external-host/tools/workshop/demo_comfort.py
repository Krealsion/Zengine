# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Check Inventory's unauthored size at both edges and in the middle of a list."""
import json
from demo_setup import ROLE
from hand import Hand
from workshop_steps import moment, picture


def run(ctx):
    link = ctx.inputs["link"]
    layout = {"zen": 1, "schema": "WorkshopSetup", "version": 3, "fields": {
        "format": "zengine-workshop-setup", "format_version": "3", "name": "Default comfort",
        "panes": [{"provider": "zengine.inventory-pane", "pane": "inventory",
                   "place": {"mode": "default", "x": "0", "y": "0"},
                   "width": {"mode": "default", "amount": "0"},
                   "height": {"mode": "default", "amount": "0"}, "front": "0"}]}}
    ctx.ask("zengine.inventory-pane", "PaneResetRequested", {"pane": "inventory"}, via=link, settle=True)
    ctx.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(layout)}, via=link, settle=True)
    hand = Hand(ctx, link)
    views = []
    try:
        for scroll in (0, -4, -100):
            if scroll:
                at = hand.view("zengine.inventory-pane", "inventory")["rows"][0]
                hand.inject([moment(ctx, "PointerWheel", wheel_dy=scroll,
                                    x=at["x"], y=at["y"], space=at["space"])])
            view = hand.view("zengine.inventory-pane", "inventory")
            text = [r["text"] for r in view["rows"]]
            ctx.check(sum("Demo input " in s for s in text) >= 3,
                      "the default pane did not show three real fixture entries: " + repr(text))
            views.append(view.fields)
        ctx.produce("views.json", json.dumps(views, indent=2).encode())
        ctx.check(any("earlier" in r["text"] for r in views[1]["rows"]) and
                  any("later" in r["text"] for r in views[1]["rows"]),
                  "the middle sample did not exercise both omission markers")
        picture(ctx, link, "default-size")
    finally:
        hand.close()
        ctx.ask(ROLE, "DemoResetRequested", {}, via=link, timeout=60)
    return "The default Inventory pane shows at least three real entries beside its headings and markers."
