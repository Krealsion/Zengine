# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Capture a new named entry, read that exact identity back, and say the label Inventory keeps it
under, as its listing says. Existing entries stay put."""
import json
from workshop_steps import link_session


def written(ctx, entry, label=None):
    said = {"reference": entry["reference"], "revision": entry["revision"]}
    if label is not None:
        said["label"] = label
    ctx.produce("entry.json", json.dumps(said, indent=2).encode("utf-8"))


def run(ctx):
    link = ctx.inputs["link"]
    link_session(ctx, link)
    ctx.step("capture new entry")
    entry = ctx.ask("zengine.inventory", "InventoryCaptureAdd", {
        "target_role": ctx.inputs["target_role"], "label": ctx.inputs.get("label", "")}, via=link)
    ctx.check(bool(entry["pair"]), "inventory returned an empty capture")
    ctx.produce("pair.bin", entry["pair"])
    ctx.step("read this entry")
    read = ctx.ask("zengine.inventory", "InventoryRead", {"reference": entry["reference"]}, via=link)
    if not (read["reference"] == entry["reference"] and read["revision"] == entry["revision"] and
            read["pair"] == entry["pair"]):
        written(ctx, entry)
        ctx.fail("the captured entry changed; pair.bin still contains this operation's own capture")
    # THE LABEL AS INVENTORY KEEPS IT: the read answers no label, so its listing says it.
    ctx.step("its label")
    listed = ctx.ask("zengine.inventory", "InventoryList", {}, via=link, version=2)
    mine = [e for e in listed["entries"] if e["reference"] == entry["reference"]]
    if not mine:
        written(ctx, entry)
        ctx.fail("Inventory's listing no longer holds the captured entry; pair.bin still contains "
                 "this operation's own capture")
    written(ctx, entry, mine[0]["label"])
    return "collected '%s' as '%s'; the same entry and revision read back (%d bytes)" % (
        ctx.inputs["target_role"], mine[0]["label"], len(entry["pair"]))
