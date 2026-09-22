# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Capture a typed snapshot, then check that Get still holds that exact pair.

The capture's own answer is saved even if another writer wins the shared slot before Get.
Read pair.bin with the installed zengine-inventory-read tool; it needs no sample schemas.
"""
from workshop_steps import link_session


def run(ctx):
    link = ctx.inputs["link"]
    target_role = ctx.inputs["target_role"]
    ctx.step("link status")
    link_session(ctx, link)
    ctx.step("capture")
    captured = ctx.ask("zengine.inventory", "InventoryCaptureDescribe",
                       {"target_role": target_role}, via=link)
    ctx.check(bool(captured["pair"]), "inventory returned an empty capture")
    ctx.produce("pair.bin", captured["pair"])
    ctx.step("get")
    state = ctx.ask("zengine.inventory", "InventoryGet", {}, via=link)
    ctx.check(state["occupied"] and state["pair"] == captured["pair"],
              "inventory changed after capture; pair.bin is this capture's snapshot, "
              "but the shared slot no longer holds it")
    return "captured '%s'; Get confirmed the same %d-byte pair, written to pair.bin" % (
        target_role, len(captured["pair"]))
