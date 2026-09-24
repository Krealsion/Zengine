# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Prepare a real command once, or exercise it after restoring into another Workshop."""
import hashlib
import json
from hand import Hand
from demo_setup import Measured, layout
from workshop_steps import picture


def run(ctx):
    measured = Measured(ctx)
    hand = Hand(measured, ctx.inputs["link"])
    phase, label = ctx.inputs["phase"], ctx.inputs["label"]
    ctx.check(phase in ("prepare", "exercise"), "phase must be prepare or exercise")
    ctx.check(label.isascii() and label.isalnum() and len(label) <= 30, "use a short ASCII word")
    office, pane = "zengine.inventory-pane", "inventory"
    command_name, output_name = label + " command", label + "Output"

    def entries():
        return hand.ask("zengine.inventory", "InventoryList", {})["entries"]

    def views():
        return hand.ask(office, "InventoryViewsRequested", {})

    def edit(operation, entry=None, view="", text="", enabled=False):
        hand.ask(office, "InventoryViewEdit", dict(operation=operation, view=view, text=text,
            entry=entry or {"owner": "", "entry": ""}, before={"owner": "", "entry": ""},
            scancode=38, modifiers=4, enabled=enabled), settle=True)  # Alt+9

    if phase == "prepare":
        ctx.check(not any(e["label"].startswith(label) for e in entries()), "choose a fresh fixture label")
        ctx.step("store a reusable command through Compose")
        setup = layout("presets")
        hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(setup)}, settle=True)
        hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": output_name})
        hand.click(hand.row("zengine.introspection", "loaded", "zengine-inventory ", scroll=True))
        hand.click(hand.row("zengine.composer", "compose", "InventoryCaptureAdd v1", scroll=True))
        hand.click(hand.row("zengine.composer", "compose", "target_role:")); hand.text("zengine.input")
        hand.click(hand.row("zengine.composer", "compose", "label:")); hand.text(output_name)
        before = entries(); hand.key("ctrl+s")
        added = [e for e in entries() if e["reference"] not in [x["reference"] for x in before]]
        ctx.check(len(added) == 1, "Compose must save one complete command")
        command = hand.ask("zengine.inventory", "InventoryRename", dict(reference=added[0]["reference"],
            revision=added[0]["revision"], label=command_name))
        hand.key("escape")
        edit("create", command["reference"], text="row")
        edit("bind", command["reference"], text="zengine.inventory")
    else:
        ctx.step("verify inactive restoration before explicitly running the saved command")
        state = views()
        ctx.check(not state["inventory_active"] and all(not v["active"] for v in state["views"])
                  and all(not b["enabled"] for b in state["bindings"]), "restore activated a saved hotkey")
        before = entries()
        command = next(e for e in before if e["label"] == command_name)
        slot = next(v for v in state["views"] if command["reference"] in v["entries"])
        setup = layout("values")
        setup["fields"]["panes"].append(dict(provider=office, pane=slot["id"],
            place=dict(mode="subcells", x="48", y=str(37 * 48)),
            width=dict(mode="subcells", amount=str(50 * 48)),
            height=dict(mode="subcells", amount=str(11 * 48)), front="3"))
        hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(setup)}, settle=True)
        count = sum(e["label"] == output_name for e in before)
        ctx.check(count == 1, "fixture should contain its original output, without an automatic replay")
        edit("enable", command["reference"], enabled=True)
        edit("context", view=slot["id"], enabled=True)
        ctx.on_cleanup(lambda: edit("context", view=slot["id"], enabled=False), "disable the fixture context")
        hand.key("alt+9")
        ctx.check(sum(e["label"] == output_name for e in entries()) == count + 1,
                  "the restored command did not create exactly one new result under current authority")
        edit("context", view=slot["id"], enabled=False)
        interior = next(r for r in hand.view(office, slot["id"])["rows"] if r["text"].startswith("|"))
        hand.drag(interior, hand.view("zengine.info", "info")["rows"][0], 350)

    current = entries()
    command = next(e for e in current if e["label"] == command_name)
    pair = hand.ask("zengine.inventory", "InventoryRead", {"reference": command["reference"]})["pair"]
    ctx.produce("fixture.json", json.dumps(dict(phase=phase, command=command,
        pair_sha256=hashlib.sha256(pair).hexdigest(), entries=current, views=views().fields,
        request_calls=dict(measured.calls)), indent=2).encode())
    picture(ctx, ctx.inputs["link"], phase)
    hand.close()
    return "Prepared a reusable command" if phase == "prepare" else "Restored command ran once with current authority"
