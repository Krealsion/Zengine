# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""Reusable demo recipes. Product state changes always go through its owners."""
import base64
from collections import Counter
import json
from pathlib import Path
import time


ROLE = "zengine.demo"
NAMES = {"values": "Value inspection", "commands": "Command reuse"}


def layout(name):
    if name not in NAMES:
        raise ValueError("unknown setup: " + name)
    rows = []

    def pane(provider, key, x, y, width, height):
        rows.append({"provider": provider, "pane": key,
                     "place": {"mode": "subcells", "x": str(x * 48), "y": str(y * 48)},
                     "width": {"mode": "subcells", "amount": str(width * 48)},
                     "height": {"mode": "subcells", "amount": str(height * 48)},
                     "front": str(len(rows))})

    pane("zengine.inventory-pane", "inventory", 1, 2, 50, 20)
    if name == "values":
        pane("zengine.info", "info", 54, 2, 64, 40)
        pane(ROLE, "controls", 1, 25, 50, 10)
    else:
        pane("zengine.introspection", "loaded", 1, 24, 50, 18)
        pane("zengine.composer", "compose", 54, 2, 64, 40)
        pane(ROLE, "controls", 1, 43, 117, 8)
    return {"zen": 1, "schema": "WorkshopSetup", "version": 3,
            "fields": {"format": "zengine-workshop-setup", "format_version": "3",
                       "name": NAMES[name], "panes": rows}}


def save_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".new")
    temporary.write_text(json.dumps(value, indent=2), encoding="utf-8")
    temporary.replace(path)


class Measured:
    def __init__(self, ctx):
        self.ctx, self.calls, self.outcomes = ctx, Counter(), Counter()

    def __getattr__(self, name):
        return getattr(self.ctx, name)

    def ask(self, role, shape, fields, **kwargs):
        self.calls[shape] += 1
        try:
            answer = self.ctx.ask(role, shape, fields, **kwargs)
            self.outcomes["answered"] += 1
            return answer
        except Exception:
            self.outcomes["failed_or_unanswered"] += 1
            raise


def prepare(ctx, name, state, link):
    """Restore only known fixture identities. Other entries, including user copies, survive."""
    class Owners:
        def ask(self, role, shape, fields, **kwargs):
            return ctx.ask(role, shape, fields, via=link, **kwargs)

        def view(self, provider, pane):
            return self.ask("zengine.workshop", "PaneViewRequested", {"provider": provider, "pane": pane})

    hand = Owners()
    targets = [("zengine.inventory-pane", "inventory")]
    targets += [("zengine.info", "info")] if name == "values" else [("zengine.composer", "compose")]
    # Refuse a pending owner operation before changing any stored fixture value.
    for role, pane in targets:
        hand.ask(role, "PaneResetRequested", {"pane": pane}, settle=True)
    hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(layout(name))}, settle=True)
    existing = list(state["fixtures"])
    if len(state["fixtures"]) < 9:
        for i in range(len(state["fixtures"]), 9):
            label = "Demo input %d" % (i + 1)
            entry = hand.ask("zengine.inventory", "InventoryCaptureAdd",
                             {"target_role": "zengine.input", "label": label}, settle=True)
            state["fixtures"].append({"reference": entry["reference"], "revision": entry["revision"], "label": label,
                                      "pair": base64.b64encode(entry["pair"]).decode()})
    if existing:
        entries = hand.ask("zengine.inventory", "InventoryList", {})["entries"]
        for fixture in existing:
            current = next((e for e in entries if e["reference"] == fixture["reference"]), None)
            if current is None:
                entry = hand.ask("zengine.inventory", "InventoryAdd", {
                    "pair": base64.b64decode(fixture["pair"]), "label": fixture["label"]}, settle=True)
                fixture["reference"] = entry["reference"]
                fixture["revision"] = entry["revision"]
                continue
            # Inventory revisions fence both value and label changes. An unchanged identity
            # at the revision we restored last time already holds the baseline.
            if current["revision"] == fixture.get("revision") and current["label"] == fixture["label"]:
                continue
            result = hand.ask("zengine.inventory", "InventoryWrite", {
                "reference": current["reference"], "revision": current["revision"],
                "pair": base64.b64decode(fixture["pair"])}, settle=True)
            if current["label"] != fixture["label"]:
                result = hand.ask("zengine.inventory", "InventoryRename", {
                    "reference": current["reference"], "revision": result["revision"],
                    "label": fixture["label"]}, settle=True)
            fixture["revision"] = result["revision"]
    # The visible reading comes from Workshop, not a private model of its typography.
    for row in layout(name)["fields"]["panes"]:
        view = hand.view(row["provider"], row["pane"])
        ctx.check(bool(view["rows"]), "a demo pane is not ready: " + row["provider"])


def serve(ctx):
    name, link = ctx.inputs["setup"], ctx.inputs["link"]
    ctx.check(name in NAMES, "unknown demo setup")
    state = {"setup": name, "fixtures": [], "samples": []}
    ctx.ask(ROLE, "DemoServiceOpened", {"name": NAMES[name]}, via=link)
    ctx.on_cleanup(lambda: ctx.ask(ROLE, "DemoServiceClosed", {}, via=link, timeout=10),
                   "detach the reset service")
    while True:
        ctx.step("waiting for initial setup or Reset demo")
        work = ctx.ask(ROLE, "DemoWorkRequested", {}, via=link, timeout=86400)
        started = time.monotonic()
        passed, note = True, "Ready; Reset restores fixture values and this layout"
        ctx.step("prepare " + name)
        measured = Measured(ctx)
        try:
            prepare(measured, name, state, link)
        except Exception as error:
            passed, note = False, str(error)
        state["samples"].append({"generation": work["generation"], "passed": passed,
                                 "elapsed_ms": (time.monotonic() - started) * 1000,
                                 "note": note, "request_calls": dict(measured.calls),
                                 "outcomes": dict(measured.outcomes)})
        state["samples"] = state["samples"][-64:]
        ctx.produce("setup.json", json.dumps(state, indent=2).encode())
        ctx.ask(ROLE, "DemoWorkFinished", {"generation": work["generation"], "passed": passed,
                                          "note": note}, via=link, settle=True)


def reset(ctx):
    started = time.monotonic()
    ctx.ask(ROLE, "DemoResetRequested", {}, via=ctx.inputs["link"], timeout=60)
    status = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(status["state"] == "ready", "reset did not produce a ready demo")
    ctx.produce("reset.json", json.dumps({"status": status.fields,
                "elapsed_ms": (time.monotonic() - started) * 1000}, indent=2).encode())
    return "Demo reset completed; unrelated entries and run evidence were retained."


def status(ctx):
    if ctx.inputs.get("wait", False):
        result = ctx.ask(ROLE, "DemoReadyRequested", {"generation": 1}, via=ctx.inputs["link"])
    else:
        result = ctx.ask(ROLE, "DemoStatusRequested", {}, via=ctx.inputs["link"])
    ctx.check(result["state"] == "ready" or not ctx.inputs.get("wait", False), result["note"])
    ctx.produce("status.json", json.dumps(result.fields, indent=2).encode())
    return result["state"] + ": " + result["note"]
