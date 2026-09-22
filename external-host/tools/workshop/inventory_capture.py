# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""workshop/inventory-capture -- ask Workshop's inventory to capture a real target's structure,
then read the stored pair back.

THE JOURNEY: the link's own session (as every tool here checks first); an InventoryCaptureDescribe
ask naming which participant's own zen.PokeStructure to capture (Workshop's inventory weave does
the asking and the storing -- this tool never builds the stored envelope itself); an InventoryGet
that reads the pair back. `zengine.inventory.InventorySet`, for a caller that already holds an
encoded pair of its own, is the generic door this journey does not need.

WHAT MOVES THE JOURNEY: only the inventory weave's own answers, through the link -- a refused
capture (a bad target_role, a target that cannot be asked) surfaces here as this run's own
failure, in the inventory's own words, never as a picture or a guess.

`pair.bin` is the whole encoded envelope Get answered with, exactly as received -- the item and
its metadata's own schema descriptors plus their native-serialized bytes (inventory/codec.hpp
owns the format; the shipped suite's decode_pair is a schema-driven, generic reader of it, no
tool of its own required to decode this file)."""

from workshop_steps import link_session


def run(ctx):
    link = ctx.inputs["link"]
    target_role = ctx.inputs["target_role"]

    ctx.step("link status")
    link_session(ctx, link)

    ctx.step("capture")
    ctx.ask("zengine.inventory", "InventoryCaptureDescribe", {"target_role": target_role},
            via=link)

    ctx.step("get")
    state = ctx.ask("zengine.inventory", "InventoryGet", {}, via=link)
    ctx.check(state["occupied"], "zengine.inventory reports the slot empty after a captured Set")
    ctx.check(len(state["pair"]) > 0, "zengine.inventory reports occupied with an empty pair")
    ctx.produce("pair.bin", state["pair"])

    return "captured '%s' into the inventory; the stored pair is %d byte(s), written to pair.bin" \
           % (target_role, len(state["pair"]))
