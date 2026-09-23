# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Capture a new named entry and read that exact identity back. Existing entries stay put."""
import json
from workshop_steps import link_session


def run(ctx):
    link = ctx.inputs["link"]
    link_session(ctx, link)
    ctx.step("capture new entry")
    entry = ctx.ask("zengine.inventory", "InventoryCaptureAdd", {
        "target_role": ctx.inputs["target_role"], "label": ctx.inputs.get("label", "")}, via=link)
    ctx.check(bool(entry["pair"]), "inventory returned an empty capture")
    ctx.produce("pair.bin", entry["pair"])
    ctx.produce("entry.json", json.dumps({"reference": entry["reference"], "revision": entry["revision"]},
                                        indent=2).encode("utf-8"))
    ctx.step("read this entry")
    read = ctx.ask("zengine.inventory", "InventoryRead", {"reference": entry["reference"]}, via=link)
    ctx.check(read["reference"] == entry["reference"] and read["revision"] == entry["revision"] and
              read["pair"] == entry["pair"],
              "the captured entry changed; pair.bin still contains this operation's own capture")
    return "collected '%s'; the same entry and revision read back (%d bytes)" % (
        ctx.inputs["target_role"], len(entry["pair"]))
