# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Portable owned slots, copied configuration and an explicitly authorized command."""
import json
from hand import Hand
from demo_setup import Measured, layout
from workshop_steps import moment, picture, chord_moments


def run(ctx):
    measured = Measured(ctx)
    hand = Hand(measured, ctx.inputs["link"])
    inv, comp = ("zengine.inventory-pane", "inventory"), ("zengine.composer", "compose")
    label = ctx.inputs["label"]

    def entries():
        return hand.ask("zengine.inventory", "InventoryList", {})["entries"]

    def state():
        return hand.ask(inv[0], "InventoryViewsRequested", {})

    def menu(row, index):
        hand.inject([moment(measured, "PointerButton", button=3, pressed=p,
                            x=row["x"], y=row["y"], space=row["space"]) for p in (True, False)])
        if index:
            hand.inject(chord_moments(measured, "down", repeat=index))
        hand.key("enter")

    ctx.check(not any(e["label"].startswith(label) for e in entries()), "choose a fresh demo label")
    ctx.check(len(state()["views"]) <= 9, "this story creates three views; use a fresh demo host after three runs")
    ctx.step("store a complete command through Compose")
    target = hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": label})
    hand.click(hand.row("zengine.introspection", "loaded", "zengine-inventory ", scroll=True))
    hand.click(hand.row(*comp, "InventoryRename v1", scroll=True))
    hand.click(hand.row(*inv, label + " : ", scroll=True)); hand.key("ctrl+enter")
    hand.click(hand.row(*comp, "reference:"))
    hand.click(hand.row(*comp, "revision:")); hand.text(str(target["revision"]))
    hand.click(hand.row(*comp, "label:")); hand.text(label + " executed")
    before = entries(); hand.key("ctrl+s")
    added = [e for e in entries() if e["reference"] not in [old["reference"] for old in before]]
    ctx.check(len(added) == 1, "Compose must save one complete command")
    command = added[0]
    hand.ask("zengine.inventory", "InventoryRename", {"reference": command["reference"],
             "revision": command["revision"], "label": label + " command"}, settle=True)
    hand.key("escape")

    def create(row, menu_index):
        before = [v["id"] for v in state()["views"]]
        menu(row, menu_index)
        created = [v for v in state()["views"] if v["id"] not in before]
        ctx.check(len(created) == 1, "pop-out did not create one view")
        return created[0]["id"]

    ctx.step("pop out a single box and configure its retained hotkey")
    box = create(hand.row(*inv, label + " command", scroll=True), 9)
    # Each creation may focus and cover another pane; layout is an explicit demo arrangement.
    def arrange(ids):
        setup = layout("presets")
        panes = setup["fields"]["panes"]
        panes[:] = [p for p in panes if p["provider"] in (inv[0], "zengine.info", "zengine.demo")]
        for p in panes:
            if p["provider"] == "zengine.info":
                p["place"] = {"mode": "subcells", "x": "48", "y": str(25 * 48)}
                p["width"]["amount"], p["height"]["amount"] = str(50 * 48), str(17 * 48)
        for key, x, y, width, height in ids:
            panes.append({"provider": inv[0], "pane": key,
                          "place": {"mode": "subcells", "x": str(x * 48), "y": str(y * 48)},
                          "width": {"mode": "subcells", "amount": str(width * 48)},
                          "height": {"mode": "subcells", "amount": str(height * 48)}, "front": str(len(panes))})
        for i, p in enumerate(panes):
            p["front"] = str(i)
        hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(setup)}, settle=True)

    views = [(box, 54, 2, 62, 7)]
    arrange(views)
    menu(hand.row(inv[0], box, label + " command"), 5)
    hand.text("zengine.inventory alt+1"); hand.key("enter")
    menu(hand.row(inv[0], box, label + " command"), 6)
    menu(hand.row(inv[0], box, label + " command"), 3)
    duplicate = next(e for e in entries() if e["label"] == label + " command 2")
    copied = next(b for b in state()["bindings"] if b["reference"] == duplicate["reference"])
    ctx.check(not copied["enabled"] and copied["target"] == "zengine.inventory" and copied["scancode"] == 30,
              "duplicate did not retain its disabled key and target")
    row = create(hand.view(*inv)["rows"][0], 1)
    def deactivate():
        hand.ask(inv[0], "InventoryViewEdit", {
            "operation": "context", "view": row, "text": "", "entry": {"owner": "", "entry": ""},
            "before": {"owner": "", "entry": ""}, "scancode": 0, "modifiers": 0, "enabled": False}, settle=True)
    ctx.on_cleanup(deactivate, "turn off this story's hotkey context")
    views.append((row, 54, 12, 62, 8)); arrange(views)
    column = create(hand.view(*inv)["rows"][0], 2)
    views.append((column, 54, 24, 62, 18)); arrange(views)

    ctx.step("move the slot, displace a filled box, and preserve its binding")
    hand.drag(hand.row(inv[0], box, label + " command"), hand.view(inv[0], row)["rows"][1], 350)
    hand.drag(hand.row(*inv, label + " : ", scroll=True), hand.view(inv[0], box)["rows"][1], 350)
    hand.drag(hand.row(inv[0], row, label + " command"), hand.row(inv[0], box, label), 350)
    ctx.check(any(e["reference"] == target["reference"] for e in entries()), "displacement lost its owned entry")
    hand.row(*inv, label + " : ", scroll=True)
    picture(ctx, ctx.inputs["link"], "displaced")
    hand.drag(hand.row(inv[0], box, label + " command"), hand.view(inv[0], row)["rows"][1], 350)
    hand.drag(hand.row(*inv, label + " : ", scroll=True), hand.view(inv[0], column)["rows"][1], 350)
    hand.drag(hand.row(*inv, label + " command 2", scroll=True), hand.view(inv[0], row)["rows"][0], 350)
    before_run = state()
    binding = next(b for b in before_run["bindings"] if b["reference"] == command["reference"])
    ctx.check(binding["enabled"] and binding["scancode"] == 30, "movement changed the item's binding")
    hand.key("alt+1")
    ctx.check(any(e["label"] == label for e in entries()), "an inactive view executed its command")
    picture(ctx, ctx.inputs["link"], "arranged")

    ctx.step("enable the row context, invoke once, and verify the owner")
    menu(hand.view(inv[0], row)["rows"][0], 3)
    hand.key("alt+1")
    changed = next(e for e in entries() if e["reference"] == target["reference"])
    ctx.check(changed["label"] == label + " executed" and changed["revision"] == target["revision"] + 1,
              "the authorized hotkey did not execute exactly once")
    picture(ctx, ctx.inputs["link"], "executed")
    # Leave no test shortcut active after this run. Stored entries and view identities remain.
    menu(hand.view(inv[0], row)["rows"][0], 3)
    final = state()
    ctx.check(not next(v for v in final["views"] if v["id"] == row)["active"], "test context did not turn off")
    ctx.produce("result.json", json.dumps({"views": final.fields, "entries": entries(),
                "request_calls": dict(measured.calls), "outcomes": dict(measured.outcomes)}, indent=2).encode())
    hand.close()
    return "Popped out three views, duplicated inactive configuration, moved and displaced owned slots, and invoked once."
