# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The inspection workbench: regenerate its packaged toolbox, restore it, or run its maker story.

Zengine owns views, sampling, edits, permissions and hit testing; this orchestrates them through
visible controls and ordinary owner requests. docs/workshop/info-views.md is the guide."""
import json
import os
import time
from demo_setup import Measured, layout
from hand import Hand
from workshop_steps import picture

PACKAGED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "toolboxes", "inspection-workbench.toolbox")
LABELS = {"sample": "Workbench sample", "note": "Workbench note",
          "preset": "Workbench capture preset", "command": "Workbench capture command"}
RESULT = "Workbench result"
INV, COMP, LOADED = ("zengine.inventory-pane", "inventory"), ("zengine.composer", "compose"), ("zengine.introspection", "loaded")
SAMPLE, PRESET, WATCH = ("zengine.info", "info"), ("zengine.info", "info.2"), ("zengine.info", "info.3")


class Owners:
    """Owner requests over the link, without an input session."""
    def __init__(self, ctx, link):
        self.ctx, self.link = ctx, link

    def ask(self, role, shape, fields, **kw):
        return self.ctx.ask(role, shape, fields, via=self.link, **kw)


def entries(hand):
    return hand.ask("zengine.inventory", "InventoryList", {})["entries"]


def named(hand, label):
    found = [e for e in entries(hand) if e["label"] == label]
    hand.ctx.check(len(found) == 1, "expected exactly one entry named " + label)
    return found[0]


def shows(hand, view, text):
    return any(text in r["text"] for r in hand.view(*view)["rows"])


def still_open(hand, view):
    """Is this pane on the desk? A closed pane's view is refused, and that refusal is the answer."""
    try:
        hand.view(*view)
        return True
    except Exception as refused:
        if "closed, unknown" in str(refused):
            return False
        raise


def expect(hand, view, text):
    hand.ctx.check(shows(hand, view, text), "%s/%s does not show %r: %s" % (
        view[0], view[1], text, [r["text"] for r in hand.view(*view)["rows"]]))


def prepare(ctx, hand, path):
    """Build the workbench material through its owners and Compose, then save it as a toolbox."""
    ctx.check(not any(e["label"].startswith("Workbench") for e in entries(hand)), "use a Workshop without workbench entries")
    ctx.step("capture a nested typed sample and a mutable note")
    hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": LABELS["sample"]})
    hand.ask("zengine.inventory", "InventoryCaptureAdd", {"target_role": "zengine.input", "label": LABELS["note"]})
    ctx.step("store an incomplete capture preset and its complete command through Compose")
    hand.click(hand.row(*LOADED, "zengine-inventory ", scroll=True))
    hand.click(hand.row(*COMP, "InventoryCaptureAdd v1", scroll=True))
    hand.click(hand.row(*COMP, "label:")); hand.text(RESULT)

    def store(chord, name):
        before = [e["reference"] for e in entries(hand)]
        hand.key(chord)
        made = [e for e in entries(hand) if e["reference"] not in before]
        ctx.check(len(made) == 1, "Compose must store exactly one entry")
        hand.ask("zengine.inventory", "InventoryRename", {"reference": made[0]["reference"],
                 "revision": made[0]["revision"], "label": name}, settle=True)

    store("ctrl+b", LABELS["preset"])
    hand.click(hand.row(*COMP, "target_role:")); hand.text("zengine.input")
    store("ctrl+s", LABELS["command"])
    hand.key("escape")
    ctx.step("save the toolbox file")
    return hand.ask("zengine.inventory-pane", "InventoryToolboxSave", {"path": path}, settle=True)


def restore(ctx, hand, path):
    """One explicit replacement, then rediscover every entry by name: no remembered identities."""
    started = time.monotonic()
    done = hand.ask("zengine.inventory-pane", "InventoryToolboxRestore", {"path": path, "replace": True}, settle=True)
    elapsed = (time.monotonic() - started) * 1000
    found = {key: named(hand, label) for key, label in LABELS.items()}
    return done, found, elapsed


def story(ctx, hand, link, pictures, shots):
    """The complete maker story, through visible Info, Inventory and Compose controls."""
    found = {key: named(hand, label) for key, label in LABELS.items()}
    results_before = sum(e["label"] == RESULT for e in entries(hand))

    ctx.step("keep the sample in one view and name the three views")
    hand.drag(hand.row(*INV, LABELS["sample"], scroll=True), hand.view(*SAMPLE)["rows"][0], 350)
    expect(hand, SAMPLE, "COPY zen.PokeStructure")
    for view, title in ((SAMPLE, "Sample"), (PRESET, "Preset"), (WATCH, "Watch")):
        hand.control(*view, "Rename"); hand.key("ctrl+a"); hand.text(title); hand.key("enter")
        expect(hand, view, title + " |")

    ctx.step("link the incomplete preset and finish it from the sample, field to field")
    hand.click(hand.row(*INV, LABELS["preset"], scroll=True)); hand.key("ctrl+enter")
    hand.click(hand.view(*PRESET)["rows"][3])
    expect(hand, PRESET, "PRESET LINKED")
    hand.field(*PRESET, "target_role")
    hand.drag(hand.field(*SAMPLE, "meta[0].requested_role"), hand.field(*PRESET, "target_role"), 500)
    expect(hand, PRESET, "*target_role: zengine.input")
    pictures.append(picture(shots, link, "filled")[0])
    hand.control(*PRESET, "Save")
    expect(hand, PRESET, "saved rev")

    ctx.step("close the preset's view and reopen it from stored data")
    hand.control(*PRESET, "Close")
    hand.control(*SAMPLE, "New")
    hand.ask("zengine.workshop", "SetupApplyRequested", {"setup": json.dumps(layout("workbench"))}, settle=True)
    expect(hand, PRESET, "Info 2 | empty")  # the same slot, a new incarnation: nothing carried over
    hand.control(*PRESET, "Rename"); hand.key("ctrl+a"); hand.text("Preset"); hand.key("enter")
    hand.click(hand.row(*INV, LABELS["preset"], scroll=True)); hand.key("ctrl+enter")
    hand.click(hand.view(*PRESET)["rows"][3])
    expect(hand, PRESET, "target_role: zengine.input")

    ctx.step("use the finished preset explicitly through Compose")
    hand.click(hand.row(*LOADED, "zengine-inventory ", scroll=True))
    hand.drag(hand.row(*INV, LABELS["preset"], scroll=True), hand.view(*COMP)["rows"][0], 350)
    expect(hand, COMP, "Copied data into form")
    hand.key("ctrl+enter")
    results = sum(e["label"] == RESULT for e in entries(hand))
    ctx.check(results == results_before + 1, "the submitted capture did not store exactly one result")
    hand.key("escape")

    ctx.step("watch a linked note while another view changes it")
    hand.click(hand.row(*INV, LABELS["note"], scroll=True)); hand.key("ctrl+enter")
    hand.click(hand.view(*WATCH)["rows"][3])
    hand.control(*WATCH, "Watch")
    expect(hand, WATCH, "watch ON")
    hand.click(hand.row(*INV, LABELS["note"], scroll=True)); hand.key("ctrl+enter")
    hand.click(hand.view(*PRESET)["rows"][3])
    edited = hand.field(*PRESET, "state_version")
    hand.click(edited); hand.key("enter"); hand.key("ctrl+a"); hand.text("7"); hand.key("enter")
    hand.control(*PRESET, "Save")
    expect(hand, WATCH, "~state_version: 7")
    pictures.append(picture(shots, link, "watching")[0])

    ctx.step("a dirty watched draft keeps its text; its stale save refuses and Save copy keeps it")
    mine = hand.field(*WATCH, "state_version")
    hand.click(mine); hand.key("enter"); hand.key("ctrl+a"); hand.text("99"); hand.key("enter")
    theirs = hand.field(*PRESET, "state_version")
    hand.click(theirs); hand.key("enter"); hand.key("ctrl+a"); hand.text("8"); hand.key("enter")
    hand.control(*PRESET, "Save")
    expect(hand, WATCH, "NEWER rev")
    expect(hand, WATCH, "*state_version: 99")
    hand.control(*WATCH, "Save")
    expect(hand, WATCH, "Save refused")
    hand.control(*WATCH, "Save copy")
    pictures.append(picture(shots, link, "conflict")[0])
    hand.control(*WATCH, "Pause")
    expect(hand, WATCH, "watch off")

    ctx.step("a whole value opened in a watching view ends that watch, at Workshop too")
    hand.control(*WATCH, "Refresh"); hand.control(*WATCH, "Refresh")  # take the newer entry: clean
    expect(hand, WATCH, "Read rev")
    hand.control(*WATCH, "Watch")
    expect(hand, WATCH, "watch ON")
    hand.drag(hand.row(*INV, LABELS["sample"], scroll=True), hand.view(*WATCH)["rows"][0], 350)
    expect(hand, WATCH, "Watch ended: this view now holds an independent copy")
    pictures.append(picture(shots, link, "custody")[0])

    ctx.step("sample the sample's source again: a new observation, stored only by Save copy")
    before = hand.ask("zengine.inventory", "InventoryRead", {"reference": found["sample"]["reference"]})["pair"]
    hand.control(*SAMPLE, "Sample")
    expect(hand, SAMPLE, "UNSAVED SAMPLE")
    after = hand.ask("zengine.inventory", "InventoryRead", {"reference": found["sample"]["reference"]})["pair"]
    ctx.check(before == after, "sampling changed the stored original")
    pictures.append(picture(shots, link, "sampled")[0])
    hand.control(*SAMPLE, "Save copy")

    ctx.step("close the watch and preset views; Inventory keeps every entry")
    count = len(entries(hand))
    for view in (WATCH, PRESET):
        hand.control(*view, "Close")
        if still_open(hand, view) and shows(hand, view, "press Close again"):
            hand.control(*view, "Close")
        ctx.check(not still_open(hand, view), "%s/%s did not close" % view)
    ctx.check(len(entries(hand)) == count, "closing a view changed Inventory")
    return {"found": found, "results": results, "entries": entries(hand)}


def run(ctx):
    phase = ctx.inputs["phase"]
    ctx.check(phase in ("prepare", "restore", "story"), "phase must be prepare, restore or story")
    path = ctx.inputs.get("path") or PACKAGED
    link = ctx.inputs["link"]
    measured, shots = Measured(ctx), Measured(ctx)
    started = time.monotonic()
    report = {"phase": phase, "path": path}
    if phase == "restore":
        # ONE OWNER REQUEST AND NO INPUT SESSION: nothing here is a maker's gesture.
        owner = Owners(measured, link)
        done, found, elapsed = restore(ctx, owner, path)
        report.update(restored=done.fields, restore_ms=elapsed, found=found,
                      elapsed_ms=(time.monotonic() - started) * 1000, maker_actions={},
                      request_calls=dict(measured.calls), outcomes=dict(measured.outcomes))
        ctx.produce("workbench.json", json.dumps(report, indent=2, default=lambda v: getattr(v, "fields", str(v))).encode())
        return "restore: %d entries, %d remote asks" % (done["entries"], sum(measured.calls.values()))
    hand = Hand(measured, link)
    if phase == "prepare":
        report["saved"] = prepare(ctx, hand, path).fields
    else:
        pictures = []
        report.update(story(ctx, hand, link, pictures, shots))
        report["pictures"] = [getattr(p, "fields", p) for p in pictures]
    report.update(elapsed_ms=(time.monotonic() - started) * 1000, maker_actions=dict(hand.actions),
                  request_calls=dict(measured.calls), outcomes=dict(measured.outcomes),
                  picture_calls=dict(shots.calls))
    hand.close()
    ctx.produce("workbench.json", json.dumps(report, indent=2, default=lambda v: getattr(v, "fields", str(v))).encode())
    return "%s: %d maker actions, %d remote asks" % (phase, sum(hand.actions.values()), sum(measured.calls.values()))
