# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Exercise copy, edit, save and fresh-read from the values recipe through visible controls."""
import json
from hand import Hand
from workshop_steps import picture


def run(ctx):
    hand = Hand(ctx, ctx.inputs["link"])
    inv, info = ("zengine.inventory-pane", "inventory"), ("zengine.info", "info")
    listed = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    fixture = next(r for r in listed if r["label"] == "Demo input 1")
    original = hand.ask("zengine.inventory", "InventoryRead", {"reference": fixture["reference"]})
    hand.drag(hand.row(*inv, "Demo input 1"), hand.view(*info)["rows"][0], 400)
    view = hand.view(*info)
    ctx.produce("opened.json", json.dumps(view.fields, indent=2).encode())
    # PokeStructure is ordinary typed data; changing a field name here does not alter a weave.
    field = hand.row(*info, "fields[0].name:")
    hand.click(field); hand.key("enter"); hand.text("demo.edited"); hand.key("enter")
    hand.key("ctrl+s")
    after = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    copies = [r for r in after if r["reference"] not in [e["reference"] for e in listed]]
    ctx.check(len(copies) == 1, "saving the detached value did not create exactly one entry")
    copy = hand.ask("zengine.inventory", "InventoryRead", {"reference": copies[0]["reference"]})
    ctx.check(copy["pair"] != original["pair"], "the edited copy retained the original bytes")
    unchanged = hand.ask("zengine.inventory", "InventoryRead", {"reference": fixture["reference"]})
    ctx.check(unchanged.fields == original.fields, "editing the copy changed its source")
    hand.key("ctrl+r")
    hand.row(*info, "demo.edited")
    picture(ctx, ctx.inputs["link"], "edited")
    ctx.produce("result.json", json.dumps({"original": fixture["reference"],
                "copy": copies[0]["reference"], "copy_revision": copy["revision"]}, indent=2).encode())
    hand.close()
    return "Dragged a value, edited and saved an independent copy, then fetched it fresh."
