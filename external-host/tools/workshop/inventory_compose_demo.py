# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Store a command, drag it back to Compose, explicitly run it and verify its owner."""
import json
from hand import Hand
from workshop_steps import link_session, picture


def run(ctx):
    link = ctx.inputs["link"]
    link_session(ctx, link)
    hand = Hand(ctx, link)
    inv, comp = ("zengine.inventory-pane", "inventory"), ("zengine.composer", "compose")
    label, renamed = ctx.inputs["label"], ctx.inputs["label"] + " renamed"
    listed = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    ctx.check(not any(r["label"] in (label, renamed) for r in listed), "demo labels already exist; choose a new label")
    source = hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": label})

    ctx.step("compose a rename using an inventory reference")
    hand.click(hand.row("zengine.introspection", "loaded", "zengine-inventory ", scroll=True))
    hand.click(hand.row(*comp, "InventoryRename v1", scroll=True))
    hand.click(hand.row(*inv, label, scroll=True))
    hand.key("ctrl+enter")
    hand.click(hand.row(*comp, "reference:"))
    hand.click(hand.row(*comp, "revision:")); hand.text(str(source["revision"]))
    hand.click(hand.row(*comp, "label:")); hand.text(renamed)
    before = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    hand.key("ctrl+s")
    after = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    created = [r for r in after if r["reference"] not in [old["reference"] for old in before]]
    ctx.check(len(created) == 1 and created[0]["schema"] == "InventoryRename", "Compose did not store one complete command")
    command = hand.ask("zengine.inventory", "InventoryRead", {"reference": created[0]["reference"]})
    ctx.produce("command.bin", command["pair"])
    picture(ctx, link, "prepared")
    hand.key("escape")

    ctx.step("drag the stored command into the empty form")
    start = hand.row(*inv, "InventoryRename", scroll=True)
    end = hand.view(*comp)["rows"][0]
    moved = hand.drag(start, end, ctx.inputs["duration_ms"], ctx.inputs["bend"],
                      during=lambda: picture(ctx, link, "dragging"))
    hand.row(*comp, "Copied data into form")
    still = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    ctx.check(any(r["reference"] == source["reference"] and r["label"] == label for r in still),
               "drop unexpectedly executed the command")
    picture(ctx, link, "dropped")
    hand.key("ctrl+enter")
    final = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
    ctx.check(any(r["reference"] == source["reference"] and r["label"] == renamed and
                  r["revision"] == source["revision"]+1 for r in final), "explicit submission did not rename the intended entry")
    saved = hand.ask("zengine.inventory", "InventoryRead", {"reference": command["reference"]})
    ctx.check(saved.fields == command.fields, "running the copied command changed its stored source")
    hand.row(*inv, renamed, scroll=True)
    picture(ctx, link, "submitted")
    ctx.produce("result.json", json.dumps({"source": source["reference"], "command": command["reference"],
                "motion": moved.fields, "entries": final}, indent=2).encode())
    hand.close()
    return "Stored, dragged and explicitly submitted a typed command; owner revision advanced once."
