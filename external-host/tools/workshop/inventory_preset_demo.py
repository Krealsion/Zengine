# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""A partial preset, filled from a captured record through the actual Info controls."""
import json
from hand import Hand
from demo_setup import Measured
from workshop_steps import picture


def run(ctx):
    counted = Measured(ctx)
    hand = Hand(counted, ctx.inputs["link"])
    inv = ("zengine.inventory-pane", "inventory")
    comp = ("zengine.composer", "compose")
    info = ("zengine.info", "info")
    label = ctx.inputs["label"]

    def entries():
        return hand.ask("zengine.inventory", "InventoryList", {})["entries"]

    def read(entry):
        return hand.ask("zengine.inventory", "InventoryRead", {"reference": entry["reference"]})

    def store(chord, name):
        before = entries()
        hand.key(chord)
        created = [e for e in entries() if e["reference"] not in [b["reference"] for b in before]]
        ctx.check(len(created) == 1, "save did not create one independent entry")
        entry = created[0]
        hand.ask("zengine.inventory", "InventoryRename", {
            "reference": entry["reference"], "revision": entry["revision"], "label": name}, settle=True)
        return read(entry)

    ctx.check(not any(e["label"].startswith(label) for e in entries()), "choose a fresh demo label")
    source = hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": label})
    ctx.produce("captured-record.bin", source["pair"])
    ctx.step("compose and save a complete command")
    hand.click(hand.row("zengine.introspection", "loaded", "zengine-inventory ", scroll=True))
    hand.click(hand.row(*comp, "InventoryRename v1", scroll=True))
    # A blank form is also saveable, without inventing any required values.
    blank = store("ctrl+b", label + " blank")
    hand.click(hand.row(*inv, label + " : ", scroll=True))
    hand.key("ctrl+enter"); hand.click(hand.row(*comp, "reference:"))
    hand.click(hand.row(*comp, "revision:")); hand.text(str(source["revision"]))
    hand.click(hand.row(*comp, "label:")); hand.text(label + " authored")
    complete = store("ctrl+s", label + " command")
    ctx.produce("complete-command.bin", complete["pair"])
    hand.key("escape")

    ctx.step("make a preset copy, unset a required value, save and reopen it")
    hand.drag(hand.row(*inv, label + " command", scroll=True), hand.view(*info)["rows"][0], 350)
    hand.key("ctrl+b")
    hand.click(hand.row(*info, "label:")); hand.key("ctrl+u")
    hand.row(*info, "label: absent (required)")
    preset = store("ctrl+s", label + " preset")
    # The label change advances the revision; fresh-read observes it through the owner.
    hand.key("ctrl+r")
    hand.row(*info, "label: absent (required)")
    ctx.produce("partial-preset.bin", preset["pair"])
    picture(ctx, ctx.inputs["link"], "incomplete")
    hand.drag(hand.row(*inv, label + " preset", scroll=True), hand.view(*comp)["rows"][0], 350)
    hand.key("ctrl+enter")
    hand.row(*comp, "still needed: label")

    ctx.step("pick up a typed field from the saved capture and fill the missing argument")
    # Inventory Capture stores the owner's typed PokeStructure answer, not its displayed text.
    hand.drag(hand.row(*inv, label + " : ", scroll=True), hand.view(*info)["rows"][0], 350)
    selected = hand.row(*info, "fields[0].name:")
    hand.click(selected); hand.key("ctrl+g")
    hand.row(*info, "Carrying field copy")
    hand.click(hand.row(*comp, "label:"))
    hand.row(*comp, "Copied data into form")
    reviewed = hand.view(*comp)
    ctx.produce("reviewed.json", json.dumps(reviewed.fields, indent=2).encode())
    before = next(e for e in entries() if e["reference"] == source["reference"])
    ctx.check(before["label"] == label and before["revision"] == source["revision"], "filling the form ran it")
    picture(ctx, ctx.inputs["link"], "refilled")

    ctx.step("explicitly submit and read the owner's actual outcome")
    hand.key("ctrl+enter")
    after = next(e for e in entries() if e["reference"] == source["reference"])
    # The visible field is only an assertion oracle; the copied payload travelled through Zengine.
    expected = selected["text"].split("fields[0].name: ", 1)[1]
    ctx.check(after["label"] == expected and after["revision"] == source["revision"] + 1,
              "submission did not apply the acquired field to the intended entry")
    ctx.check(read(complete)["pair"] == complete["pair"], "complete command source changed")
    ctx.check(read(preset)["pair"] == preset["pair"], "partial preset source changed")
    ctx.check(read(source)["pair"] == source["pair"], "captured record data changed")
    # Replaying a saved revision must refuse rather than silently refresh and retry.
    hand.key("ctrl+enter")
    hand.row(*comp, "entry changed")
    final = entries()
    unchanged = next(e for e in final if e["reference"] == source["reference"])
    ctx.check(unchanged == after, "stale revision unexpectedly changed the owner")
    hand.close()
    ctx.produce("result.json", json.dumps({"source": source["reference"], "complete": complete["reference"],
        "preset": preset["reference"], "blank": blank["reference"], "outcome": after, "entries": final,
        "request_calls": dict(counted.calls), "outcomes": dict(counted.outcomes)}, indent=2).encode())
    return "Saved blank and partial presets, filled from a captured typed field, and explicitly applied one command."
